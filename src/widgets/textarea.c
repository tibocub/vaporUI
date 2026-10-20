#include <stdlib.h>
#include <string.h>

#include "../../include/vaporui.h"
#include "../utf8.h"
#include "util.h"

#define NONE ((unsigned int)-1)
#define HISTORY_MAX_CHARS 65536 // shortcut: history is dropped entirely past this, no partial trimming
#define HISTORY_MAX_EDITS 4096

// -- History --
// Linear list of edits; edits[0..done) can be undone, [done..count) redone.
// Each edit replaced `del` characters at `pos` with `ins` characters; both
// strings live in `pool` (deleted first, then inserted) starting at `off`.

typedef struct {
    unsigned int pos, del, ins, off;
    int mergeable; // made by plain typing / Backspace / Delete, so the next one may join it
} Edit;

struct VuiHistory {
    Edit *edits;
    unsigned int count, done, cap;
    VuiCodepoint *pool;
    unsigned int pool_len, pool_cap;
};

static void history_clear(VuiHistory *h)
{
    h->count = h->done = h->pool_len = 0;
}

static int pool_reserve(VuiHistory *h, unsigned int extra)
{
    if (h->pool_len + extra > h->pool_cap) {
        unsigned int cap = (h->pool_len + extra) * 2;
        VuiCodepoint *pool = realloc(h->pool, cap * sizeof(VuiCodepoint));

        if (pool == NULL) {
            return 0;
        }

        h->pool = pool;
        h->pool_cap = cap;
    }

    return 1;
}

// -- Text queries --

static unsigned int line_start(const VuiTextarea *t, unsigned int i)
{
    while (i > 0 && t->text[i - 1] != '\n') i--;
    return i;
}

static unsigned int line_end(const VuiTextarea *t, unsigned int i)
{
    while (i < t->len && t->text[i] != '\n') i++;
    return i;
}

#define cw cell_w

static unsigned int cols(const VuiTextarea *t, unsigned int from, unsigned int to)
{
    return cells(t->text, from, to);
}

// Width the text wraps at; one column is left free for the cursor at a line end.
static unsigned int wrap_width(const VuiTextarea *t)
{
    unsigned int w = t->node.rect.width;

    return w > 1 ? w - 1 : 1;
}

// End (exclusive) of the visual row starting at s. A row break falls after the
// last space that fits, or mid-word when the word is longer than a row.
static unsigned int row_end(const VuiTextarea *t, unsigned int s)
{
    unsigned int le = line_end(t, s), i = s, c = 0, after_space = 0, w = wrap_width(t);

    if (!t->wrap) return le;

    for (; i < le && c + cw(t->text[i]) <= w; i++) {
        c += cw(t->text[i]);
        if (t->text[i] == ' ') after_space = i + 1;
    }

    if (i == le) return le;
    if (t->text[i] == ' ') return i + 1; // the space that doesn't fit hangs at the row's end
    if (after_space > s) return after_space;
    return i > s ? i : s + 1;
}

static unsigned int row_start(const VuiTextarea *t, unsigned int i)
{
    unsigned int s = line_start(t, i), le = line_end(t, i);

    for (;;) {
        unsigned int e = row_end(t, s);

        if (i < e || e >= le) return s;
        s = e;
    }
}

static unsigned int next_row(const VuiTextarea *t, unsigned int s)
{
    unsigned int e = row_end(t, s), le = line_end(t, s);

    if (e < le) return e;
    return le < t->len ? le + 1 : NONE;
}

// Last cursor position on the row [s, e): the end of a wrapped row belongs to the next row.
static unsigned int row_limit(const VuiTextarea *t, unsigned int s)
{
    unsigned int e = row_end(t, s);

    return e < line_end(t, s) ? e - 1 : e;
}

// Index in the row starting at s that is at (or just before) display column `col`.
static unsigned int index_at(const VuiTextarea *t, unsigned int s, unsigned int col)
{
    unsigned int limit = row_limit(t, s), c = 0;

    while (s < limit && c + cw(t->text[s]) <= col) c += cw(t->text[s++]);
    return s;
}

static unsigned int is_space(VuiCodepoint c) { return c == ' ' || c == '\n'; }

static unsigned int word_left(const VuiTextarea *t)
{
    unsigned int i = t->cursor;

    while (i > 0 && is_space(t->text[i - 1])) i--;
    while (i > 0 && !is_space(t->text[i - 1])) i--;
    return i;
}

static unsigned int word_right(const VuiTextarea *t)
{
    unsigned int i = t->cursor;

    while (i < t->len && is_space(t->text[i])) i++;
    while (i < t->len && !is_space(t->text[i])) i++;
    return i;
}

static unsigned int sel_lo(const VuiTextarea *t) { return t->cursor < t->anchor ? t->cursor : t->anchor; }
static unsigned int sel_hi(const VuiTextarea *t) { return t->cursor < t->anchor ? t->anchor : t->cursor; }

// -- Editing primitives --

static int reserve(VuiTextarea *t, unsigned int n)
{
    if (n > t->cap) {
        unsigned int cap = t->cap ? t->cap * 2 : 64;
        VuiCodepoint *text;

        while (cap < n) cap *= 2;
        text = realloc(t->text, cap * sizeof(VuiCodepoint));

        if (text == NULL) return 0;

        t->text = text;
        t->cap = cap;
    }

    return 1;
}

static void splice(VuiTextarea *t, unsigned int pos, unsigned int del, const VuiCodepoint *ins, unsigned int n)
{
    if (!reserve(t, t->len - del + n)) return;

    memmove(t->text + pos + n, t->text + pos + del, (t->len - pos - del) * sizeof(VuiCodepoint));
    if (n > 0) memcpy(t->text + pos, ins, n * sizeof(VuiCodepoint));
    t->len = t->len - del + n;
    t->changed = 1;
    t->reveal = 1;
}

// Replace `del` characters at pos with ins[0..n). `merge` lets a run of typing or
// of Backspace/Delete become one undo step. Returns how many characters were
// inserted (max_chars can cut it short). The cursor is left to the caller.
static unsigned int replace(VuiTextarea *t, unsigned int pos, unsigned int del, const VuiCodepoint *ins, unsigned int n, int merge)
{
    VuiHistory *h = t->history;

    if (t->max != 0 && t->len - del + n > t->max) {
        n = t->max > t->len - del ? t->max - (t->len - del) : 0;
    }

    if (del == 0 && n == 0) return 0;

    if (h == NULL) h = t->history = calloc(1, sizeof(*h));

    if (h != NULL) {
        Edit *last = h->done > 0 ? &h->edits[h->done - 1] : NULL;

        h->count = h->done;
        h->pool_len = last != NULL ? last->off + last->del + last->ins : 0;

        if (h->pool_len > HISTORY_MAX_CHARS || h->count >= HISTORY_MAX_EDITS) {
            history_clear(h);
            last = NULL;
        }

        if (!pool_reserve(h, del + n)) {
            history_clear(h);
        } else if (merge && last != NULL && last->mergeable && del == 0 && last->del == 0 && pos == last->pos + last->ins) {
            memcpy(h->pool + h->pool_len, ins, n * sizeof(VuiCodepoint));
            last->ins += n;
            h->pool_len += n;
        } else if (merge && last != NULL && last->mergeable && n == 0 && last->ins == 0 && pos + del == last->pos) {
            // Backspace run: the new text goes before what was deleted so far
            memmove(h->pool + last->off + del, h->pool + last->off, last->del * sizeof(VuiCodepoint));
            memcpy(h->pool + last->off, t->text + pos, del * sizeof(VuiCodepoint));
            last->pos = pos;
            last->del += del;
            h->pool_len += del;
        } else if (merge && last != NULL && last->mergeable && n == 0 && last->ins == 0 && pos == last->pos) {
            memcpy(h->pool + h->pool_len, t->text + pos, del * sizeof(VuiCodepoint));
            last->del += del;
            h->pool_len += del;
        } else {
            if (h->count == h->cap) {
                unsigned int cap = h->cap ? h->cap * 2 : 32;
                Edit *edits = realloc(h->edits, cap * sizeof(Edit));

                if (edits == NULL) {
                    history_clear(h);
                    goto apply;
                }

                h->edits = edits;
                h->cap = cap;
            }

            memcpy(h->pool + h->pool_len, t->text + pos, del * sizeof(VuiCodepoint));
            memcpy(h->pool + h->pool_len + del, ins, n * sizeof(VuiCodepoint));
            h->edits[h->count++] = (Edit){ pos, del, n, h->pool_len, merge };
            h->pool_len += del + n;
            h->done = h->count;
        }
    }

apply:
    splice(t, pos, del, ins, n);
    return n;
}

static void history_step(VuiTextarea *t, int redo)
{
    VuiHistory *h = t->history;
    Edit *e;

    if (h == NULL || (redo ? h->done == h->count : h->done == 0)) return;

    e = &h->edits[redo ? h->done++ : --h->done];

    if (redo) {
        splice(t, e->pos, e->del, h->pool + e->off + e->del, e->ins);
        t->cursor = e->pos + e->ins;
    } else {
        splice(t, e->pos, e->ins, h->pool + e->off, e->del);
        t->cursor = e->pos + e->del;
    }

    t->anchor = t->cursor;
}

static void delete_range(VuiTextarea *t, unsigned int from, unsigned int to, int merge)
{
    replace(t, from, to - from, NULL, 0, merge);
    t->cursor = t->anchor = from;
}

// Types ins[0..n), replacing the selection if there is one.
static void put(VuiTextarea *t, const VuiCodepoint *ins, unsigned int n, int merge)
{
    unsigned int lo = sel_lo(t), hi = sel_hi(t);

    n = lo != hi ? replace(t, lo, hi - lo, ins, n, 0) : replace(t, lo, 0, ins, n, merge);
    t->cursor = t->anchor = lo + n;
}

static void put_tab(VuiTextarea *t)
{
    VuiCodepoint spaces[16];
    unsigned int n = t->tab_width - (t->cursor - line_start(t, t->cursor)) % t->tab_width;

    if (n > 16) n = 16;
    for (unsigned int i = 0; i < n; i++) spaces[i] = ' ';
    put(t, spaces, n, 0);
}

static void put_newline(VuiTextarea *t)
{
    VuiCodepoint buf[128] = { '\n' };
    unsigned int n = 1;

    if (t->auto_indent) {
        for (unsigned int i = line_start(t, sel_lo(t)); i < sel_lo(t) && n < 128 && (t->text[i] == ' ' || t->text[i] == '\t'); i++) {
            buf[n++] = t->text[i];
        }
    }

    put(t, buf, n, 0);
}

static void copy_selection(const VuiTextarea *t)
{
    unsigned int lo = sel_lo(t), n = sel_hi(t) - lo;
    char *utf8 = malloc(n * 4 + 1);

    if (utf8 == NULL) return;

    vui_cps_to_utf8(t->text + lo, n, utf8, n * 4 + 1);
    vui_clipboard_set(utf8);
    free(utf8);
}

static void paste_clipboard(VuiTextarea *t)
{
    const char *s = vui_clipboard_get();
    VuiCodepoint *cps = malloc((strlen(s) + 1) * sizeof(VuiCodepoint));

    if (cps == NULL) return;

    put(t, cps, vui_utf8_to_cps(s, cps, (unsigned int)strlen(s)), 0);
    free(cps);
}

// -- Cursor movement --

static void move_up(VuiTextarea *t)
{
    unsigned int s = row_start(t, t->cursor);

    if (s > 0) t->cursor = index_at(t, row_start(t, s - 1), t->want);
}

static void move_down(VuiTextarea *t)
{
    unsigned int n = next_row(t, row_start(t, t->cursor));

    if (n != NONE) t->cursor = index_at(t, n, t->want);
}

static void scroll_rows(VuiTextarea *t, int rows)
{
    for (; rows < 0 && t->top > 0; rows++) t->top = row_start(t, t->top - 1);

    for (; rows > 0; rows--) {
        unsigned int n = next_row(t, t->top);

        if (n == NONE) break;
        t->top = n;
    }
}

// Returns 1 if the text changed or 0 if not, or -1 if the key isn't ours.
// `vertical` is set for moves that keep the remembered column.
static int edit_key(VuiTextarea *t, const VuiKeyEvent *k, int *vertical)
{
    int shift = (k->modifiers & VUI_MOD_SHIFT) != 0;
    VuiModifiers m = k->modifiers & ~VUI_MOD_SHIFT;
    int word = m == VUI_MOD_CTRL || m == VUI_MOD_ALT;
    unsigned int page = t->node.rect.height > 1 ? t->node.rect.height - 1 : 1;
    int selected = t->cursor != t->anchor;

    if (m == VUI_MOD_ALT && k->key == VUI_KEY_CHAR && k->character == 'a') {
        textarea_select_all(t);
        return 0;
    }

    if (m == VUI_MOD_CTRL && k->key == VUI_KEY_CHAR) {
        switch (k->character) {
        case 'z': if (shift) history_step(t, 1); else history_step(t, 0); return 0;
        case 'y': history_step(t, 1); return 0;
        case 'c': if (!selected) return -1; copy_selection(t); return 0;
        case 'x': if (!selected) return 0; copy_selection(t); delete_range(t, sel_lo(t), sel_hi(t), 0); return 1;
        case 'v': paste_clipboard(t); return 1;
        case 'e': t->cursor = t->anchor = row_limit(t, row_start(t, t->cursor)); return 0;
        case 'u': delete_range(t, selected ? sel_lo(t) : line_start(t, t->cursor), selected ? sel_hi(t) : t->cursor, 0); return 1;
        case 'w': case 'h': delete_range(t, selected ? sel_lo(t) : word_left(t), selected ? sel_hi(t) : t->cursor, 0); return 1;
        case 'k':
            if (selected) delete_range(t, sel_lo(t), sel_hi(t), 0);
            else if (t->cursor < t->len) delete_range(t, t->cursor, line_end(t, t->cursor) > t->cursor ? line_end(t, t->cursor) : t->cursor + 1, 0);
            return 1;
        default: return -1;
        }
    }

    if (m && !word) return -1;

    switch (k->key) {
    case VUI_KEY_INSERT:
        if (m == VUI_MOD_CTRL && selected) { copy_selection(t); return 0; }
        if (shift && !m) { paste_clipboard(t); return 1; }
        return -1;

    case VUI_KEY_LEFT:
        if (selected && !shift) { t->cursor = sel_lo(t); t->anchor = t->cursor; return 0; }
        t->cursor = word ? word_left(t) : (t->cursor ? t->cursor - 1 : 0);
        break;

    case VUI_KEY_RIGHT:
        if (selected && !shift) { t->cursor = sel_hi(t); t->anchor = t->cursor; return 0; }
        t->cursor = word ? word_right(t) : (t->cursor < t->len ? t->cursor + 1 : t->len);
        break;

    case VUI_KEY_UP:   *vertical = 1; move_up(t); break;
    case VUI_KEY_DOWN: *vertical = 1; move_down(t); break;
    case VUI_KEY_HOME: t->cursor = word ? 0 : row_start(t, t->cursor); break;
    case VUI_KEY_END:  t->cursor = word ? t->len : row_limit(t, row_start(t, t->cursor)); break;

    case VUI_KEY_PAGE_UP:
        *vertical = 1;
        for (unsigned int i = 0; i < page; i++) move_up(t);
        break;

    case VUI_KEY_PAGE_DOWN:
        *vertical = 1;
        for (unsigned int i = 0; i < page; i++) move_down(t);
        break;

    case VUI_KEY_ENTER:
        if (m || shift) return -1;
        put_newline(t);
        return 1;

    case VUI_KEY_TAB:
        if (m || shift) return -1; // Shift-Tab leaves the field
        put_tab(t);
        return 1;

    case VUI_KEY_BACKSPACE:
        if (selected) delete_range(t, sel_lo(t), sel_hi(t), 0);
        else delete_range(t, word ? word_left(t) : (t->cursor ? t->cursor - 1 : 0), t->cursor, !word);
        return 1;

    case VUI_KEY_DELETE:
        if (shift && !m) { if (selected) { copy_selection(t); delete_range(t, sel_lo(t), sel_hi(t), 0); } return 1; }
        if (selected) delete_range(t, sel_lo(t), sel_hi(t), 0);
        else delete_range(t, t->cursor, word ? word_right(t) : (t->cursor < t->len ? t->cursor + 1 : t->len), !word);
        return 1;

    default:
        return -1;
    }

    // a movement key: Shift extends the selection, otherwise it collapses
    if (!shift) t->anchor = t->cursor;
    return 0;
}

// Display column -> index for mouse positions; x/y are relative to the node and may be outside it.
static unsigned int index_from_mouse(VuiTextarea *t, long x, long y)
{
    unsigned int s = t->top, h = t->node.rect.height;

    if (y < 0) { scroll_rows(t, -1); s = t->top; y = 0; }
    else if (h > 0 && y >= (long)h) { scroll_rows(t, 1); s = t->top; y = h - 1; }

    for (; y > 0; y--) {
        unsigned int n = next_row(t, s);

        if (n == NONE) break;
        s = n;
    }

    return index_at(t, s, (t->wrap ? 0 : t->left) + (x < 0 ? 0 : (unsigned int)x));
}

static int textarea_event(VuiNode *node, VuiCtx *ui, const VuiEvent *event)
{
    VuiTextarea *t = (VuiTextarea *)node;
    int result = -1, vertical = 0;

    (void)ui;

    switch (event->type) {
    case VUI_EVENT_KEY:
        result = edit_key(t, &event->key, &vertical);
        break;

    case VUI_EVENT_TEXT: {
        VuiCodepoint cp = event->text.codepoint;

        if (cp == '\t') {
            put_tab(t);
        } else if (cp == '\n') {
            VuiCodepoint nl = '\n';

            put(t, &nl, 1, 0); // a pasted newline: no auto-indent
        } else if (cp >= 32 && cp != 127) {
            put(t, &cp, 1, 1);
        }

        result = 1;
        break;
    }

    case VUI_EVENT_MOUSE:
        if (event->mouse.button == VUI_MOUSE_WHEEL_UP || event->mouse.button == VUI_MOUSE_WHEEL_DOWN) {
            scroll_rows(t, event->mouse.button == VUI_MOUSE_WHEEL_UP ? -3 : 3);
            return 1;
        }

        if (event->mouse.button == VUI_MOUSE_LEFT && event->mouse.action != VUI_MOUSE_RELEASE) {
            unsigned int i = index_from_mouse(t, (long)event->mouse.x - (long)node->rect.x, (long)event->mouse.y - (long)node->rect.y);

            t->cursor = i;

            if (event->mouse.action == VUI_MOUSE_PRESS && !(event->mouse.modifiers & VUI_MOD_SHIFT)) {
                t->anchor = i;
            }

            t->want = cols(t, row_start(t, i), i);
            return 1;
        }
        return event->mouse.button == VUI_MOUSE_LEFT;

    default:
        return 0;
    }

    if (result < 0) {
        return 0;
    }

    if (!vertical) {
        t->want = cols(t, row_start(t, t->cursor), t->cursor);
    }

    t->reveal = 1;

    if (t->changed) {
        t->changed = 0;

        if (t->on_change != NULL) {
            t->on_change(node);
        }
    }

    return 1;
}

// -- Drawing --

static void reveal(VuiTextarea *t, unsigned int w, unsigned int h)
{
    unsigned int crow = row_start(t, t->cursor), n = 0, s;

    if (t->top > t->len) t->top = t->len;
    t->top = row_start(t, t->top);

    if (crow < t->top) {
        t->top = crow;
    }

    for (s = t->top; s != crow && s != NONE; s = next_row(t, s)) n++;

    for (; n >= h; n--) t->top = next_row(t, t->top);

    if (!t->wrap) {
        unsigned int col = cols(t, line_start(t, t->cursor), t->cursor);

        if (col < t->left) {
            t->left = col;
        } else if (col + 1 > t->left + w) {
            t->left = col + 1 - w;
        }
    }
}

static void textarea_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiTextarea *t = (VuiTextarea *)node;
    VuiRect r = node->rect;
    VuiStyle plain = vui_theme()->text, inverse = vui_theme()->selection;
    unsigned int lo = sel_lo(t), hi = sel_hi(t), s;

    (void)userdata;

    if (r.width == 0 || r.height == 0) {
        return;
    }

    if (t->reveal) {
        reveal(t, r.width, r.height);
        t->reveal = 0;
    }

    s = t->top;

    for (unsigned int row = 0; row < r.height && s != NONE; row++) {
        unsigned int e = row_end(t, s), le = line_end(t, s), c = 0, skip = t->wrap ? 0 : t->left;

        for (unsigned int i = s; i < e; i++) {
            int selected = i >= lo && i < hi;
            VuiStyle st = selected ? inverse : plain;

            if (node->focused && i == t->cursor) {
                st = (VuiStyle){ .attr = selected ? VUI_ATTR_REVERSE | VUI_ATTR_UNDERLINE : VUI_ATTR_REVERSE };
            }

            if (c >= skip && c - skip < r.width) {
                scr_set(scr, r.x + c - skip, r.y + row, t->text[i] == '\t' ? ' ' : t->text[i], st);
            }

            c += cw(t->text[i]);
        }

        // Past the last character of a line: the cursor, or a selected newline, shows as a blank cell.
        if (e == le && c >= skip && c - skip < r.width) {
            int newline_selected = e < t->len && e >= lo && e < hi;

            if (node->focused && e == t->cursor) {
                scr_set(scr, r.x + c - skip, r.y + row, ' ', (VuiStyle){ .attr = newline_selected ? VUI_ATTR_REVERSE | VUI_ATTR_UNDERLINE : VUI_ATTR_REVERSE });
            } else if (newline_selected) {
                scr_set(scr, r.x + c - skip, r.y + row, ' ', inverse);
            }
        }

        s = next_row(t, s);
    }
}

static void textarea_destroy(VuiNode *node)
{
    VuiTextarea *t = (VuiTextarea *)node;

    if (t->history != NULL) {
        free(t->history->edits);
        free(t->history->pool);
        free(t->history);
    }

    free(t->text);
}

// -- Public API --

VuiTextarea *textarea_create(unsigned int max_chars)
{
    VuiTextarea *t = (VuiTextarea *)node_alloc(sizeof(VuiTextarea));

    if (t != NULL) {
        t->max = max_chars;
        t->tab_width = 4;
        t->wrap = 1;
        t->node.grow = 1;
        t->node.focusable = 1;
        t->node.wants_tab = 1;
        t->node.draw = textarea_draw;
        t->node.on_event = textarea_event;
        t->node.destroy = textarea_destroy;
    }

    return t;
}

void textarea_set_text(VuiTextarea *t, const char *utf8)
{
    unsigned int n = (unsigned int)strlen(utf8); // codepoints <= bytes

    if (t->max != 0 && n > t->max) n = t->max;
    if (!reserve(t, n)) return;

    t->len = vui_utf8_to_cps(utf8, t->text, n);
    t->cursor = t->anchor = t->want = t->top = t->left = 0;
    t->reveal = 1;

    if (t->history != NULL) history_clear(t->history);
}

size_t textarea_get_text(const VuiTextarea *t, char *out, size_t size)
{
    return vui_cps_to_utf8(t->text, t->len, out, size);
}

size_t textarea_get_selection(const VuiTextarea *t, char *out, size_t size)
{
    return vui_cps_to_utf8(t->text + sel_lo(t), sel_hi(t) - sel_lo(t), out, size);
}

void textarea_select_all(VuiTextarea *t)
{
    t->anchor = 0;
    t->cursor = t->len;
    t->reveal = 1;
}

void textarea_undo(VuiTextarea *t) { history_step(t, 0); }
void textarea_redo(VuiTextarea *t) { history_step(t, 1); }

void textarea_cursor_pos(const VuiTextarea *t, unsigned int *line, unsigned int *column)
{
    unsigned int l = 0;

    for (unsigned int i = 0; i < t->cursor; i++) {
        l += t->text[i] == '\n';
    }

    *line = l;
    *column = t->cursor - line_start(t, t->cursor);
}
