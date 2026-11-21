// Terminal widget: a child process on a pty, its output interpreted by libvterm (third_party/libvterm).
// POSIX (forkpty) and NuttX (/dev/ptmx + posix_spawn: no MMU, no fork); not part of the default
// build, see the Makefile and vaporOS/vaporui.
#ifndef __NuttX__
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#endif
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__NuttX__)
#include <fcntl.h>
#include <spawn.h>
#elif defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

#include "../../include/vaporui.h"
#include "../../third_party/libvterm/include/vterm.h"
#include "../ctx.h"
#include "../utf8.h"
#include "termsel.h"

// Scrollback: lines that scroll off the top (or off a shrinking window) wait here, in a ring, so that scrolling the
// view or making the window taller or wider again brings them back. Each line is stored at the width it had.
// shortcut: they don't reflow themselves (only the visible rows do). VUI_TERMINAL_SCROLLBACK (vaporui.h) = 0 builds none of it.
typedef struct {
    int cols;
    int cont; // this line continues the previous one (a soft wrap)
    VTermScreenCell cells[];
} Line;

#if VUI_TERMINAL_SCROLLBACK > 0
#define SBN VUI_TERMINAL_SCROLLBACK

static Line **sb_slot(const VuiTerminal *t, unsigned int i)
{
    return &((Line **)t->sb)[(t->sb_head + i) % SBN];
}

static int sb_push(int cols, const VTermScreenCell *cells, bool cont, void *user)
{
    VuiTerminal *t = user;
    Line *l = malloc(sizeof(Line) + cols * sizeof(VTermScreenCell));

    if (l == NULL) return 0;
    l->cols = cols;
    l->cont = cont;
    memcpy(l->cells, cells, cols * sizeof(VTermScreenCell));

    if (t->sb == NULL && (t->sb = calloc(SBN, sizeof(Line *))) == NULL) { free(l); return 0; }

    if (t->sb_len == SBN) {
        free(*sb_slot(t, 0));
        t->sb_head = (t->sb_head + 1) % SBN;
        t->sb_len--;
        t->sb_base++;
    }

    *sb_slot(t, t->sb_len++) = l;
    if (t->scroll > 0) t->scroll = ts_scroll(t->scroll, 1, t->sb_len); // stay on the same text
    return 1;
}

// Hands back the last row of the newest line. libvterm asks with the width the screen had before a resize and
// then cuts the row to the new one, so a line wider than the new width (t->cols, set before the resize) comes
// back in pieces of that width, last piece first: it re-wraps instead of being cut off.
static int sb_pop(int cols, VTermScreenCell *cells, void *user)
{
    VuiTerminal *t = user;
    Line *l;
    int used = 0, start = 0;

    if (t->sb_len == 0) return 0;

    l = *sb_slot(t, t->sb_len - 1);

    for (int i = 0; i < l->cols; i++) {
        if (l->cells[i].chars[0] != 0) used = i + (l->cells[i].width > 1 ? l->cells[i].width : 1);
    }

    int w = t->cols > 0 && (int)t->cols < cols ? (int)t->cols : cols;

    if (used > w) start = (used - 1) / w * w;

    for (int i = 0; i < cols; i++) {
        if (start + i < l->cols) {
            cells[i] = l->cells[start + i];
        } else {
            memset(&cells[i], 0, sizeof(cells[i]));
            cells[i].width = 1;
            cells[i].fg = l->cells[l->cols - 1].fg;
            cells[i].bg = l->cells[l->cols - 1].bg;
        }
    }

    if (start > 0) {
        l->cols = start; // the rest of the line stays for the next pop
    } else {
        free(l);
        t->sb_len--;
        t->scroll = ts_scroll(t->scroll, 0, t->sb_len);
    }

    return 1;
}

static int sb_clear(void *user)
{
    VuiTerminal *t = user;

    while (t->sb_len > 0) free(*sb_slot(t, --t->sb_len));
    t->sb_head = 0;
    t->scroll = 0;
    t->sel_on = 0;
    return 1;
}
#else
#define sb_push NULL
#define sb_pop NULL
#define sb_clear NULL
#endif

void terminal_scroll(VuiTerminal *t, int rows)
{
    if (VUI_TERMINAL_SCROLLBACK > 0 && !t->altscreen) t->scroll = ts_scroll(t->scroll, rows, t->sb_len);
}

void terminal_scroll_reset(VuiTerminal *t) { t->scroll = 0; }
unsigned int terminal_scroll_offset(const VuiTerminal *t) { return t->scroll; }
void terminal_selection_clear(VuiTerminal *t) { t->sel_on = t->selecting = 0; }

// The cell at column x of view line `idx` (an index: below sb_len a scrollback line, then the screen rows);
// returns 0 for a blank beyond the line's end.
static int cell_at(const VuiTerminal *t, long idx, int x, VTermScreenCell *c)
{
#if VUI_TERMINAL_SCROLLBACK > 0
    if (idx < (long)t->sb_len) {
        const Line *l = *sb_slot(t, idx);

        if (x >= l->cols) return 0;
        *c = l->cells[x];
        return 1;
    }
#endif
    if (x >= (int)t->cols) return 0;
    vterm_screen_get_cell(t->screen, (VTermPos){ idx - t->sb_len, x }, c);
    return 1;
}

static int line_width(const VuiTerminal *t, long idx)
{
#if VUI_TERMINAL_SCROLLBACK > 0
    if (idx < (long)t->sb_len) return (*sb_slot(t, idx))->cols;
#endif
    return t->cols;
}

// does view line idx continue the one above it (the program's line wrapped)?
static int is_cont(const VuiTerminal *t, long idx)
{
#if VUI_TERMINAL_SCROLLBACK > 0
    if (idx < (long)t->sb_len) return (*sb_slot(t, idx))->cont;
#endif
    return idx - (long)t->sb_len < (long)t->rows && vterm_state_get_lineinfo(vterm_obtain_state(t->vt), idx - t->sb_len)->continuation;
}

static int blank(const VTermScreenCell *c) { return c->chars[0] == 0 || c->chars[0] == ' '; }

size_t terminal_selection_text(const VuiTerminal *t, char *out, size_t size)
{
    TsPos a = { t->sel_line[0], t->sel_col[0] }, b = { t->sel_line[1], t->sel_col[1] };
    size_t n = 0;
    long lo, hi;
    int pending = 0;

    if (!t->sel_on) {
        if (size > 0) out[0] = 0;
        return 0;
    }

    if (ts_cmp(a, b) > 0) { TsPos x = a; a = b; b = x; }
    lo = a.line - t->sb_base;
    hi = b.line - t->sb_base;
    if (lo < 0) lo = 0; // dropped off the scrollback since

    for (long idx = lo; idx <= hi && idx < (long)t->sb_len + (long)t->rows; idx++) {
        int c0, c1;

        // a soft-wrapped row is full: its trailing blanks are real, and no newline joins it to the next
        if (idx > lo && !is_cont(t, idx)) { pending = 0; if (n + 1 < size) out[n] = '\n'; n++; }
        if (!ts_span(a, b, idx + t->sb_base, line_width(t, idx), &c0, &c1)) continue;

        for (int x = c0; x < c1; x++) {
            VTermScreenCell c;
            uint8_t enc[4];

            if (!cell_at(t, idx, x, &c) || c.chars[0] == (uint32_t)-1) continue;
            if (blank(&c)) { pending++; continue; } // trailing blanks are dropped, so they wait

            for (; pending > 0; pending--) { if (n + 1 < size) out[n] = ' '; n++; }

            for (int k = 0; k < VTERM_MAX_CHARS_PER_CELL && c.chars[k] != 0; k++) {
                int len = vui_utf8_encode(c.chars[k], enc);

                for (int m = 0; m < len; m++) { if (n + 1 < size) out[n] = (char)enc[m]; n++; }
            }
        }
    }

    if (size > 0) out[n < size ? n : size - 1] = 0;
    return n;
}

void terminal_copy(VuiTerminal *t)
{
    size_t n = terminal_selection_text(t, NULL, 0);
    char *buf = n > 0 ? malloc(n + 1) : NULL;

    if (buf == NULL) return;
    terminal_selection_text(t, buf, n + 1);
    vui_clipboard_set(buf);
    free(buf);
}

static VuiColor color(VTermScreen *screen, VTermColor c)
{
    if (VTERM_COLOR_IS_DEFAULT_FG(&c) || VTERM_COLOR_IS_DEFAULT_BG(&c)) return VUI_COLOR_DEFAULT;
    if (VTERM_COLOR_IS_INDEXED(&c)) return c.indexed.idx < 16 ? (VuiColor)c.indexed.idx + 1 : VUI_COLOR256(c.indexed.idx);
    vterm_screen_convert_color_to_rgb(screen, &c);
    return VUI_RGB(c.rgb.red, c.rgb.green, c.rgb.blue);
}

static void term_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiTerminal *t = (VuiTerminal *)node;
    VuiRect r = node->rect;
    VTermPos cur;
    VuiStyle blank = vui_theme()->text;

    (void)userdata;

    // follow the node's size: the vterm and the child's idea of the window must match what we show
    if (r.width > 0 && r.height > 0 && (r.width != t->cols || r.height != t->rows)) {
        struct winsize ws = { .ws_row = r.height, .ws_col = r.width };

        t->cols = r.width;
        t->rows = r.height;
        t->sel_on = t->selecting = 0; // rows re-wrap: the selection's lines no longer match
        vterm_set_size(t->vt, t->rows, t->cols);
        if (t->fd >= 0) ioctl(t->fd, TIOCSWINSZ, &ws);
    }

    scr_fill(scr, r, ' ', blank);

    for (unsigned int y = 0; y < r.height && y < t->rows; y++) {
        long idx = (long)t->sb_len - (long)t->scroll + y;
        int c0 = 0, c1 = 0, sel = t->sel_on && ts_span((TsPos){ t->sel_line[0], t->sel_col[0] }, (TsPos){ t->sel_line[1], t->sel_col[1] },
                                                       idx + t->sb_base, r.width, &c0, &c1);

        for (unsigned int x = 0; x < r.width; x++) {
            VTermScreenCell c;
            VuiStyle s = { 0 };
            int selected = sel && (int)x >= c0 && (int)x < c1;

            if (!cell_at(t, idx, x, &c) || c.chars[0] == 0 || c.chars[0] == (uint32_t)-1) c.chars[0] = 0; // blank, or the right half of a wide char
            if (c.chars[0] == 0 && !selected) continue;

            if (c.chars[0] != 0 || selected) {
                s.fg = color(t->screen, c.fg);
                s.bg = color(t->screen, c.bg);
                s.attr = (c.attrs.bold ? VUI_ATTR_BOLD : 0) | (c.attrs.underline ? VUI_ATTR_UNDERLINE : 0) | (c.attrs.reverse ? VUI_ATTR_REVERSE : 0);
                if (selected) s.attr ^= VUI_ATTR_REVERSE;
            }

            // shortcut: only the base character is drawn, combining marks are dropped
            scr_set(scr, r.x + x, r.y + y, c.chars[0] ? c.chars[0] : ' ', s);
        }
    }

    if (t->scroll > 0) {
        char tag[16];
        int n = snprintf(tag, sizeof(tag), "[%u]", t->scroll);

        if ((unsigned)n <= r.width) scr_puts(scr, r.x + r.width - n, r.y, tag, (VuiStyle){ .attr = VUI_ATTR_REVERSE });
    }

    vterm_state_get_cursorpos(vterm_obtain_state(t->vt), &cur);

    if (node->focused && t->cursor_visible && t->scroll == 0 && (unsigned)cur.col < r.width && (unsigned)cur.row < r.height) {
        VTermScreenCell c;
        VuiStyle s = { .attr = VUI_ATTR_REVERSE };

        vterm_screen_get_cell(t->screen, cur, &c);
        scr_set(scr, r.x + cur.col, r.y + cur.row, c.chars[0] && c.chars[0] != (uint32_t)-1 ? c.chars[0] : ' ', s);
    }
}

static int wordch(const VuiTerminal *t, long idx, int x)
{
    VTermScreenCell c;

    if (!cell_at(t, idx, x, &c) || blank(&c)) return 0;
    return c.chars[0] > 127 || isalnum((int)c.chars[0]) || strchr("_-./~@%+=#", (int)c.chars[0]) != NULL; // also the right half of a wide char (-1)
}

// double click: the word under the cell; triple: the program's line, wrapped rows included
static void select_unit(VuiTerminal *t, int clicks, int y, int x)
{
    long idx = (long)t->sb_len - (long)t->scroll + y, last;
    int l = x, r = x;

    if (clicks == 2) {
        if (wordch(t, idx, x)) {
            while (l > 0 && wordch(t, idx, l - 1)) l--;
            while (r + 1 < line_width(t, idx) && wordch(t, idx, r + 1)) r++;
        }

        t->sel_line[0] = t->sel_line[1] = idx + t->sb_base;
        t->sel_col[0] = l;
        t->sel_col[1] = r;
    } else {
        last = idx;
        while (idx > 0 && is_cont(t, idx)) idx--;
        while (last + 1 < (long)t->sb_len + (long)t->rows && is_cont(t, last + 1)) last++;
        t->sel_line[0] = idx + t->sb_base;
        t->sel_col[0] = 0;
        t->sel_line[1] = last + t->sb_base;
        t->sel_col[1] = line_width(t, last) - 1;
    }

    t->sel_on = 1;
}

// while the button is held outside the node, scroll steadily instead of once per mouse event
static void drag_tick(VuiCtx *ui, void *user)
{
    VuiTerminal *t = user;

    if (!t->selecting || t->drag_dir == 0) {
        vui_timer_cancel(ui, t->drag_timer);
        t->drag_timer = 0;
        return;
    }

    terminal_scroll(t, t->drag_dir);
    t->sel_line[1] = ts_row_id(t->sb_base, t->sb_len, t->scroll, t->drag_dir > 0 ? 0 : (int)t->rows - 1);
    t->sel_col[1] = t->drag_x;
}

// Mouse: to the program when it asked for it, else select (left drag) and scroll (wheel). A gesture that started
// one way finishes that way, whatever Shift does meanwhile.
static int term_mouse(VuiTerminal *t, const VuiMouseEvent *m)
{
    VTermModifier mod = (m->modifiers & VUI_MOD_SHIFT ? VTERM_MOD_SHIFT : 0) | (m->modifiers & VUI_MOD_ALT ? VTERM_MOD_ALT : 0)
                      | (m->modifiers & VUI_MOD_CTRL ? VTERM_MOD_CTRL : 0);
    long rx = (long)m->x - t->node.rect.x, ry = (long)m->y - t->node.rect.y;
    int x = rx < 0 ? 0 : rx >= (long)t->cols ? (int)t->cols - 1 : (int)rx;
    int y = ry < 0 ? 0 : ry >= (long)t->rows ? (int)t->rows - 1 : (int)ry;
    int wheel = m->button == VUI_MOUSE_WHEEL_UP || m->button == VUI_MOUSE_WHEEL_DOWN;
    int want = !t->selecting && (t->fwd || (t->mouse_mode != 0 && !(m->modifiers & VUI_MOD_SHIFT)));

    if (t->fd < 0 && want) want = 0;

    if (want) {
        int b = m->button == VUI_MOUSE_LEFT ? 1 : m->button == VUI_MOUSE_MIDDLE ? 2 : m->button == VUI_MOUSE_RIGHT ? 3
              : m->button == VUI_MOUSE_WHEEL_UP ? 4 : m->button == VUI_MOUSE_WHEEL_DOWN ? 5 : 0;

        vterm_mouse_move(t->vt, y, x, mod);
        if (b > 0 && m->action != VUI_MOUSE_MOVE) {
            vterm_mouse_button(t->vt, b, m->action == VUI_MOUSE_PRESS, mod);
            if (b <= 3) t->fwd = m->action == VUI_MOUSE_PRESS;
        }
        return b > 0 || m->action == VUI_MOUSE_MOVE;
    }

    if (wheel) {
        terminal_scroll(t, m->button == VUI_MOUSE_WHEEL_UP ? 3 : -3);
        return 1;
    }

    if (m->button != VUI_MOUSE_LEFT) return 0;

    if (m->action == VUI_MOUSE_RELEASE) {
        t->selecting = t->drag_dir = 0;
        return 1;
    }

    if (m->action == VUI_MOUSE_MOVE) {
        t->drag_dir = !t->selecting ? 0 : ry < 0 ? 1 : ry >= (long)t->rows ? -1 : 0;
        t->drag_x = x;

        if (t->drag_dir != 0 && t->drag_timer == 0) t->drag_timer = vui_timer(vui_ctx(), 60, 1, drag_tick, t);
    }

    t->sel_line[1] = ts_row_id(t->sb_base, t->sb_len, t->scroll, y);
    t->sel_col[1] = x;

    if (m->action == VUI_MOUSE_PRESS) {
        uint64_t now = vui_platform_now_ms();

        t->clicks = now - t->click_ms < 400 && x == t->click_x && y == t->click_y ? t->clicks % 3 + 1 : 1;
        t->click_ms = now;
        t->click_x = x;
        t->click_y = y;
        t->selecting = 1;
        t->sel_on = 0;
        t->sel_line[0] = t->sel_line[1];
        t->sel_col[0] = x;
        if (t->clicks > 1) select_unit(t, t->clicks, y, x);
    } else if (t->selecting) {
        t->sel_on = 1;
    }

    return 1;
}

void terminal_paste(VuiTerminal *t)
{
    const char *p = vui_clipboard_get();
    uint32_t *cps = t->fd >= 0 && *p ? malloc((strlen(p) + 1) * sizeof(uint32_t)) : NULL;
    unsigned int n;

    if (cps == NULL) return;
    n = vui_utf8_to_cps(p, cps, strlen(p) + 1);
    t->scroll = 0;
    vterm_keyboard_start_paste(t->vt);

    // controls other than Tab and Enter are dropped: an ESC in the text must not end a bracketed paste early or start a sequence
    for (unsigned int i = 0; i < n; i++) {
        uint32_t c = cps[i] == '\n' ? '\r' : cps[i];

        if (c >= 0x20 || c == '\r' || c == '\t') vterm_keyboard_unichar(t->vt, c, VTERM_MOD_NONE);
    }

    vterm_keyboard_end_paste(t->vt);
    free(cps);
}

static int term_event(VuiNode *node, VuiCtx *ui, const VuiEvent *e)
{
    VuiTerminal *t = (VuiTerminal *)node;
    VTermModifier mod = VTERM_MOD_NONE;

    (void)ui;

    if (e->type == VUI_EVENT_MOUSE) return term_mouse(t, &e->mouse);

    if (e->type == VUI_EVENT_KEY) {
        const VuiKeyEvent *k = &e->key;

        if (t->on_key != NULL && t->on_key(t, k)) return 1;

        if (k->modifiers == VUI_MOD_SHIFT && !t->altscreen && VUI_TERMINAL_SCROLLBACK > 0) {
            switch (k->key) {
            case VUI_KEY_PAGE_UP:   terminal_scroll(t, (int)t->rows > 1 ? (int)t->rows - 1 : 1); return 1;
            case VUI_KEY_PAGE_DOWN: terminal_scroll(t, -((int)t->rows > 1 ? (int)t->rows - 1 : 1)); return 1;
            case VUI_KEY_HOME:      terminal_scroll(t, (int)t->sb_len); return 1;
            case VUI_KEY_END:       terminal_scroll_reset(t); return 1;
            default: break;
            }
        }

        // legacy terminals can't tell Ctrl-Shift-C from Ctrl-C (0x03): that one reaches the program
        if (k->modifiers == (VUI_MOD_CTRL | VUI_MOD_SHIFT) && k->key == VUI_KEY_CHAR && (k->character == 'c' || k->character == 'v')) {
            if (k->character == 'c') terminal_copy(t); else terminal_paste(t);
            return 1;
        }
    }

    if (t->fd < 0) return 0;

    switch (e->type) {
    case VUI_EVENT_PASTE_START: t->scroll = 0; t->pasting = 1; vterm_keyboard_start_paste(t->vt); return 1;
    case VUI_EVENT_PASTE_END:   t->pasting = 0; vterm_keyboard_end_paste(t->vt); return 1;
    case VUI_EVENT_TEXT:        t->scroll = 0; vterm_keyboard_unichar(t->vt, t->pasting && e->text.codepoint == '\n' ? '\r' : e->text.codepoint, VTERM_MOD_NONE); return 1;
    case VUI_EVENT_KEY:         t->scroll = 0; break;
    default:                    return 0;
    }

    if (e->key.modifiers & VUI_MOD_SHIFT) mod |= VTERM_MOD_SHIFT;
    if (e->key.modifiers & VUI_MOD_ALT)   mod |= VTERM_MOD_ALT;
    if (e->key.modifiers & VUI_MOD_CTRL)  mod |= VTERM_MOD_CTRL;

    if (e->key.key == VUI_KEY_CHAR) {
        vterm_keyboard_unichar(t->vt, e->key.character, mod);
        return 1;
    }

    if (e->key.key >= VUI_KEY_F1 && e->key.key <= VUI_KEY_F12) {
        vterm_keyboard_key(t->vt, VTERM_KEY_FUNCTION(e->key.key - VUI_KEY_F1 + 1), mod);
        return 1;
    }

    static const struct { VuiKey from; VTermKey to; } map[] = {
        { VUI_KEY_ENTER, VTERM_KEY_ENTER }, { VUI_KEY_BACKSPACE, VTERM_KEY_BACKSPACE }, { VUI_KEY_TAB, VTERM_KEY_TAB },
        { VUI_KEY_ESCAPE, VTERM_KEY_ESCAPE }, { VUI_KEY_DELETE, VTERM_KEY_DEL }, { VUI_KEY_UP, VTERM_KEY_UP },
        { VUI_KEY_DOWN, VTERM_KEY_DOWN }, { VUI_KEY_LEFT, VTERM_KEY_LEFT }, { VUI_KEY_RIGHT, VTERM_KEY_RIGHT },
        { VUI_KEY_HOME, VTERM_KEY_HOME }, { VUI_KEY_END, VTERM_KEY_END }, { VUI_KEY_PAGE_UP, VTERM_KEY_PAGEUP },
        { VUI_KEY_PAGE_DOWN, VTERM_KEY_PAGEDOWN }, { VUI_KEY_INSERT, VTERM_KEY_INS },
    };

    // Shift-Tab is left to the focus system (F6 also moves out), like the text area
    if (e->key.key == VUI_KEY_TAB && (mod & VTERM_MOD_SHIFT)) return 0;

    for (unsigned int i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (map[i].from == e->key.key) {
            vterm_keyboard_key(t->vt, map[i].to, mod);
            return 1;
        }
    }

    return 0;
}

// libvterm -> child: keyboard input and replies to queries (cursor position, ...)
static void to_child(const char *s, size_t len, void *user)
{
    VuiTerminal *t = user;

    while (t->fd >= 0 && len > 0) {
        ssize_t n = write(t->fd, s, len);

        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break; // shortcut: a full pty buffer drops the input, when a child stops reading
        s += n;
        len -= n;
    }
}

static int set_prop(VTermProp prop, VTermValue *val, void *user)
{
    VuiTerminal *t = user;

    if (prop == VTERM_PROP_CURSORVISIBLE) t->cursor_visible = val->boolean;
    if (prop == VTERM_PROP_MOUSE) t->mouse_mode = val->number;
    if (prop == VTERM_PROP_ALTSCREEN) {
        t->altscreen = val->boolean;
        t->scroll = 0;
        t->sel_on = t->selecting = 0; // screen rows are a different set of lines now
    }

    return 1;
}

static const VTermScreenCallbacks screen_cb = { .settermprop = set_prop, .sb_pushline4 = sb_push, .sb_popline = sb_pop, .sb_clear = sb_clear };

static void child_gone(VuiTerminal *t)
{
    int status = 0;

    vui_unwatch_fd(vui_ctx(), t->fd);
    close(t->fd);
    t->fd = -1;
    waitpid(t->pid, &status, 0);
    t->pid = 0;
    if (t->on_exit != NULL) t->on_exit(&t->node, status);
}

static void from_child(VuiCtx *ui, int fd, void *user)
{
    VuiTerminal *t = user;
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf));

    (void)ui;

    if (n > 0) {
        vterm_input_write(t->vt, buf, n);
        vterm_screen_flush_damage(t->screen);
    } else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
        child_gone(t);
    }
}

static void term_destroy(VuiNode *node)
{
    VuiTerminal *t = (VuiTerminal *)node;

    if (t->fd >= 0) {
        vui_unwatch_fd(vui_ctx(), t->fd);
        close(t->fd);
        kill(t->pid, SIGHUP);
#ifdef __NuttX__
        // closing the master hangs the pty up: the shell sees EOF and exits. shortcut: not waited for
        // (a stuck child must not freeze the UI), so its exit status stays allocated until the app exits
        waitpid(t->pid, NULL, WNOHANG);
#else
        waitpid(t->pid, NULL, 0); // shortcut: a child ignoring SIGHUP blocks here, when that matters send SIGKILL after a timeout
#endif
    }

    if (t->drag_timer != 0) vui_timer_cancel(vui_ctx(), t->drag_timer);
    vterm_free(t->vt);
    #if VUI_TERMINAL_SCROLLBACK > 0
    sb_clear(t);
#endif
    free(t->sb);
}

VuiTerminal *terminal_create(const char *const argv[])
{
    VuiTerminal *t = (VuiTerminal *)node_alloc(sizeof(VuiTerminal));
    struct winsize ws = { .ws_row = 24, .ws_col = 80 };
    const char *sh[] = { getenv("SHELL") ? getenv("SHELL") : "/bin/sh", NULL };

    if (t == NULL) return NULL;
    if (argv == NULL) argv = sh;

    t->rows = 24;
    t->cols = 80;
    t->cursor_visible = 1;
    t->vt = vterm_new(t->rows, t->cols);

    if (t->vt == NULL) {
        node_destroy(&t->node);
        return NULL;
    }

    vterm_set_utf8(t->vt, 1);
    vterm_output_set_callback(t->vt, to_child, t);
    t->screen = vterm_obtain_screen(t->vt);
    vterm_screen_set_callbacks(t->screen, &screen_cb, t);
    vterm_screen_callbacks_has_pushline4(t->screen);
    vterm_screen_enable_altscreen(t->screen, 1);
    vterm_screen_enable_reflow(t->screen, 1); // a resize re-wraps the rows that are shown
    vterm_screen_reset(t->screen, 1);

    setenv("TERM", "xterm-256color", 1);

#ifdef __NuttX__
    {
        char slave[32];
        posix_spawn_file_actions_t fa;
        int ok = 0;

        t->fd = open("/dev/ptmx", O_RDWR | O_NOCTTY);

        if (t->fd >= 0 && unlockpt(t->fd) == 0 && ptsname_r(t->fd, slave, sizeof(slave)) == 0) {
            ioctl(t->fd, TIOCSWINSZ, (unsigned long)&ws);
            posix_spawn_file_actions_init(&fa);
            posix_spawn_file_actions_addopen(&fa, 0, slave, O_RDWR, 0);
            posix_spawn_file_actions_adddup2(&fa, 0, 1);
            posix_spawn_file_actions_adddup2(&fa, 0, 2);
            ok = posix_spawnp(&t->pid, argv[0], &fa, NULL, (char *const *)argv, NULL) == 0;
            posix_spawn_file_actions_destroy(&fa);
        }

        if (!ok) {
            if (t->fd >= 0) close(t->fd);
            t->fd = -1;
        }

        t->pid = ok ? t->pid : -1;
    }
#else
    t->pid = forkpty(&t->fd, NULL, NULL, &ws);

    if (t->pid == 0) {
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
#endif

    if (t->pid < 0) {
        t->fd = -1;
        t->node.destroy = term_destroy; // frees the vterm
        node_destroy(&t->node);
        return NULL;
    }

    t->node.destroy = term_destroy;
    t->node.draw = term_draw;
    t->node.on_event = term_event;
    t->node.focusable = 1;
    t->node.wants_tab = 1;
    t->node.grow = 1;
    vui_watch_fd(vui_ctx(), t->fd, from_child, t);
    return t;
}
