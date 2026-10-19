#include <stdlib.h>
#include <string.h>

#include "../../include/vaporui.h"
#include "../utf8.h"

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

static int is_space(VuiCodepoint c)
{
    return c == ' ' || c == '\n';
}

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

static void insert(VuiTextarea *t, VuiCodepoint cp)
{
    if (t->max != 0 && t->len >= t->max) {
        return;
    }

    if (t->len == t->cap) {
        unsigned int cap = t->cap ? t->cap * 2 : 64;
        VuiCodepoint *text = realloc(t->text, cap * sizeof(VuiCodepoint));

        if (text == NULL) {
            return;
        }

        t->text = text;
        t->cap = cap;
    }

    memmove(t->text + t->cursor + 1, t->text + t->cursor, (t->len - t->cursor) * sizeof(VuiCodepoint));
    t->text[t->cursor++] = cp;
    t->len++;
}

static void erase(VuiTextarea *t, unsigned int from, unsigned int to)
{
    memmove(t->text + from, t->text + to, (t->len - to) * sizeof(VuiCodepoint));
    t->len -= to - from;
    t->cursor = from;
}

static void move_up(VuiTextarea *t)
{
    unsigned int ls = line_start(t, t->cursor);

    if (ls > 0) {
        unsigned int prev = line_start(t, ls - 1);

        t->cursor = prev + t->want < ls - 1 ? prev + t->want : ls - 1;
    }
}

static void move_down(VuiTextarea *t)
{
    unsigned int le = line_end(t, t->cursor);

    if (le < t->len) {
        unsigned int next_end = line_end(t, le + 1);

        t->cursor = le + 1 + t->want < next_end ? le + 1 + t->want : next_end;
    }
}

static void scroll_lines(VuiTextarea *t, int lines)
{
    for (; lines < 0 && t->top > 0; lines++) {
        t->top = line_start(t, t->top - 1);
    }

    for (; lines > 0 && line_end(t, t->top) < t->len; lines--) {
        t->top = line_end(t, t->top) + 1;
    }
}

// Returns 1 if the text changed, 0 if not, -1 if the key isn't ours. `vertical` is set
// for moves that keep the remembered column.
static int edit_key(VuiTextarea *t, const VuiKeyEvent *k, int *vertical)
{
    VuiModifiers m = k->modifiers;
    int word = m == VUI_MOD_CTRL || m == VUI_MOD_ALT;
    unsigned int page = t->node.rect.height > 1 ? t->node.rect.height - 1 : 1;
    unsigned int col = t->cursor - line_start(t, t->cursor);

    if (m == VUI_MOD_CTRL && k->key == VUI_KEY_CHAR) {
        switch (k->character) {
        case 'a': t->cursor -= col; return 0;
        case 'e': t->cursor = line_end(t, t->cursor); return 0;
        case 'u': erase(t, t->cursor - col, t->cursor); return 1;
        case 'w': case 'h': erase(t, word_left(t), t->cursor); return 1;
        case 'k':
            if (t->cursor < t->len) {
                unsigned int le = line_end(t, t->cursor);

                erase(t, t->cursor, le > t->cursor ? le : le + 1);
                return 1;
            }
            return 0;
        default: return -1;
        }
    }

    if (m && !word && k->key != VUI_KEY_TAB) {
        return -1;
    }

    switch (k->key) {
    case VUI_KEY_LEFT:  t->cursor = word ? word_left(t) : (t->cursor ? t->cursor - 1 : 0); return 0;
    case VUI_KEY_RIGHT: t->cursor = word ? word_right(t) : (t->cursor < t->len ? t->cursor + 1 : t->len); return 0;
    case VUI_KEY_UP:    *vertical = 1; move_up(t); return 0;
    case VUI_KEY_DOWN:  *vertical = 1; move_down(t); return 0;
    case VUI_KEY_HOME:  t->cursor = word ? 0 : t->cursor - col; return 0;
    case VUI_KEY_END:   t->cursor = word ? t->len : line_end(t, t->cursor); return 0;

    case VUI_KEY_PAGE_UP:
        *vertical = 1;
        for (unsigned int i = 0; i < page; i++) move_up(t);
        return 0;

    case VUI_KEY_PAGE_DOWN:
        *vertical = 1;
        for (unsigned int i = 0; i < page; i++) move_down(t);
        return 0;

    case VUI_KEY_ENTER:
        if (m) return -1;
        insert(t, '\n');
        return 1;

    case VUI_KEY_TAB:
        if (m) return -1; // Shift-Tab leaves the field
        for (unsigned int n = t->tab_width - col % t->tab_width; n > 0; n--) insert(t, ' ');
        return 1;

    case VUI_KEY_BACKSPACE:
        erase(t, word ? word_left(t) : (t->cursor ? t->cursor - 1 : 0), t->cursor);
        return 1;

    case VUI_KEY_DELETE:
        erase(t, t->cursor, word ? word_right(t) : (t->cursor < t->len ? t->cursor + 1 : t->len));
        return 1;

    default:
        return -1;
    }
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
            unsigned int col = t->cursor - line_start(t, t->cursor);

            for (unsigned int n = t->tab_width - col % t->tab_width; n > 0; n--) insert(t, ' ');
        } else if ((cp >= 32 && cp != 127) || cp == '\n') {
            insert(t, cp);
        }

        result = 1;
        break;
    }

    case VUI_EVENT_MOUSE:
        if (event->mouse.button == VUI_MOUSE_WHEEL_UP || event->mouse.button == VUI_MOUSE_WHEEL_DOWN) {
            scroll_lines(t, event->mouse.button == VUI_MOUSE_WHEEL_UP ? -3 : 3);
            return 1;
        }

        if (event->mouse.action == VUI_MOUSE_PRESS && event->mouse.button == VUI_MOUSE_LEFT) {
            unsigned int p = t->top, le;

            for (unsigned int row = event->mouse.y - node->rect.y; row > 0; row--) {
                le = line_end(t, p);

                if (le >= t->len) break;
                p = le + 1;
            }

            le = line_end(t, p);
            t->cursor = p + t->left + (event->mouse.x - node->rect.x);

            if (t->cursor > le) t->cursor = le;

            t->want = t->cursor - p;
            return 1;
        }
        return 0;

    default:
        return 0;
    }

    if (result < 0) {
        return 0;
    }

    if (!vertical) {
        t->want = t->cursor - line_start(t, t->cursor);
    }

    t->reveal = 1;

    if (result == 1 && t->on_change != NULL) {
        t->on_change(node);
    }

    return 1;
}

static void reveal(VuiTextarea *t, unsigned int w, unsigned int h)
{
    unsigned int ls = line_start(t, t->cursor);
    unsigned int col = t->cursor - ls;
    unsigned int lines = 0;

    if (t->top > t->len) t->top = t->len;
    t->top = line_start(t, t->top);

    if (t->cursor < t->top) {
        t->top = ls;
    }

    for (unsigned int i = t->top; i < t->cursor; i++) {
        lines += t->text[i] == '\n';
    }

    for (; lines >= h; lines--) {
        t->top = line_end(t, t->top) + 1;
    }

    if (col < t->left) {
        t->left = col;
    } else if (col - t->left >= w) {
        t->left = col - w + 1;
    }
}

static void textarea_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiTextarea *t = (VuiTextarea *)node;
    VuiRect r = node->rect;
    VuiStyle plain = {0}, cursor = { .attr = VUI_ATTR_REVERSE };
    unsigned int p;

    (void)userdata;

    if (r.width == 0 || r.height == 0) {
        return;
    }

    if (t->reveal) {
        reveal(t, r.width, r.height);
        t->reveal = 0;
    }

    p = t->top;

    for (unsigned int row = 0; row < r.height; row++) {
        unsigned int le = line_end(t, p), s = p;

        for (unsigned int skip = t->left; skip > 0 && s < le; skip--) s++;

        for (unsigned int i = 0; i < r.width && s + i < le; i++) {
            scr_set(scr, r.x + i, r.y + row, t->text[s + i], plain);
        }

        if (node->focused && t->cursor >= p && t->cursor <= le && t->cursor >= p + t->left) {
            unsigned int c = t->cursor - p - t->left;

            scr_set(scr, r.x + c, r.y + row, t->cursor < le ? t->text[t->cursor] : ' ', cursor);
        }

        if (le >= t->len) break;

        p = le + 1;
    }
}

static void textarea_destroy(VuiNode *node)
{
    free(((VuiTextarea *)node)->text);
}

VuiTextarea *textarea_create(unsigned int max_chars)
{
    VuiTextarea *t = (VuiTextarea *)node_alloc(sizeof(VuiTextarea));

    if (t != NULL) {
        t->max = max_chars;
        t->tab_width = 4;
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

    if (t->max != 0 && n > t->max) {
        n = t->max;
    }

    if (n > t->cap) {
        VuiCodepoint *text = realloc(t->text, n * sizeof(VuiCodepoint));

        if (text == NULL) {
            return;
        }

        t->text = text;
        t->cap = n;
    }

    t->len = vui_utf8_to_cps(utf8, t->text, n);
    t->cursor = t->want = t->top = t->left = 0;
    t->reveal = 1;
}

size_t textarea_get_text(const VuiTextarea *t, char *out, size_t size)
{
    return vui_cps_to_utf8(t->text, t->len, out, size);
}

void textarea_cursor_pos(const VuiTextarea *t, unsigned int *line, unsigned int *column)
{
    unsigned int l = 0;

    for (unsigned int i = 0; i < t->cursor; i++) {
        l += t->text[i] == '\n';
    }

    *line = l;
    *column = t->cursor - line_start(t, t->cursor);
}
