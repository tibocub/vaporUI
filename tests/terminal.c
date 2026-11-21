// Terminal widget: scrollback view, selection, copy, key hooks. Runs a real child on a pty. `make test` (POSIX).
#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../include/vaporui.h"

extern unsigned int stub_ready_mask;
extern uint64_t stub_now;

static VuiTerminal *t;
static VuiCtx *ui;
static int hooked;

static int hook(VuiTerminal *term, const VuiKeyEvent *k)
{
    (void)term;
    hooked++;
    return k->key == VUI_KEY_F5; // F5 is the app's
}

static void mouse(VuiMouseButton b, VuiMouseAction a, unsigned x, unsigned y, VuiModifiers m)
{
    VuiEvent e = { .type = VUI_EVENT_MOUSE, .mouse = { b, a, m, x, y } };

    assert(t->node.on_event(&t->node, ui, &e));
}

static int key(VuiKey k, VuiModifiers m, VuiCodepoint ch)
{
    VuiEvent e = { .type = VUI_EVENT_KEY, .key = { .key = k, .modifiers = m, .character = ch } };

    return t->node.on_event(&t->node, ui, &e);
}

// select the rows' first N columns from (0,y0) to (n-1,y1) with the mouse, read it back, return the first line number
static int first_line(unsigned y0, unsigned y1, char *out, size_t size)
{
    int n = 0;

    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_PRESS, 0, y0, 0);
    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_MOVE, 3, y1, 0);
    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_RELEASE, 3, y1, 0);
    terminal_selection_text(t, out, size);
    sscanf(out, "line%d", &n);
    return n;
}

int main(void)
{
    char a[256], b[256];
    VuiScreen *scr;
    int live, back;

    ui = vui_init();
    scr = scr_create(20, 10);
    t = terminal_create((const char *const[]){ "sh", "-c", "i=1; while [ $i -le 60 ]; do echo line$i; i=$((i+1)); done; sleep 5", NULL });
    assert(t != NULL);
    t->on_key = hook;
    t->node.rect = (VuiRect){ 0, 0, 20, 10 };

    usleep(400000);
    stub_ready_mask = 1;
    vui_next_event(ui, &t->node);
    stub_ready_mask = 0;
    node_draw(&t->node, scr); // sizes the vterm to 20x10
    assert(t->rows == 10 && t->sb_len > 40 && t->scroll == 0);

    live = first_line(0, 2, a, sizeof a);
    assert(live > 40 && strchr(a, '\n') != NULL);

    terminal_scroll(t, 5);
    assert(t->scroll == 5 && terminal_scroll_offset(t) == 5);
    back = first_line(0, 2, b, sizeof b);
    assert(back == live - 5); // same screen position, 5 lines further back
    terminal_scroll(t, 100000);
    assert(t->scroll == t->sb_len); // clamped
    terminal_scroll(t, -100000);
    assert(t->scroll == 0);

    // copy: selection -> clipboard, exactly the selection text
    first_line(0, 2, a, sizeof a);
    terminal_copy(t);
    assert(strcmp(vui_clipboard_get(), a) == 0);
    assert(key(VUI_KEY_CHAR, VUI_MOD_CTRL | VUI_MOD_SHIFT, 'c') && hooked > 0);

    // a drag selection keeps its text while the view scrolls (ids stay with the content)
    first_line(0, 2, a, sizeof a);
    terminal_scroll(t, 7);
    terminal_selection_text(t, b, sizeof b);
    assert(strcmp(a, b) == 0);
    terminal_scroll_reset(t);

    // keys: Shift-PageUp scrolls a page, Shift-Home the top, Shift-End live; a typed key snaps back; the hook can take a key
    assert(key(VUI_KEY_PAGE_UP, VUI_MOD_SHIFT, 0) && t->scroll == 9);
    assert(key(VUI_KEY_HOME, VUI_MOD_SHIFT, 0) && t->scroll == t->sb_len);
    assert(key(VUI_KEY_END, VUI_MOD_SHIFT, 0) && t->scroll == 0);
    terminal_scroll(t, 4);
    key(VUI_KEY_CHAR, VUI_MOD_CTRL, 'd');
    assert(t->scroll == 0);
    terminal_scroll(t, 4);
    assert(key(VUI_KEY_F5, 0, 0) && t->scroll == 4); // consumed by the hook: not sent, no snap
    mouse(VUI_MOUSE_WHEEL_UP, VUI_MOUSE_PRESS, 5, 5, 0);
    assert(t->scroll == 7);
    mouse(VUI_MOUSE_WHEEL_DOWN, VUI_MOUSE_PRESS, 5, 5, 0);
    assert(t->scroll == 4);

    terminal_selection_clear(t);
    assert(terminal_selection_text(t, a, sizeof a) == 0 && a[0] == 0);

    node_destroy(&t->node);

    // a program that enabled mouse reporting (SGR) gets press/release/wheel; Shift selects instead
    t = terminal_create((const char *const[]){ "sh", "-c", "printf '\\033[?1000h\\033[?1006h'; cat -v", NULL });
    assert(t != NULL);
    t->node.rect = (VuiRect){ 0, 0, 20, 10 };
    node_draw(&t->node, scr);
    usleep(300000);
    stub_ready_mask = 1;
    vui_next_event(ui, &t->node);
    assert(t->mouse_mode != 0);
    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_PRESS, 2, 1, 0);
    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_RELEASE, 2, 1, 0);
    mouse(VUI_MOUSE_WHEEL_UP, VUI_MOUSE_PRESS, 2, 1, 0);
    key(VUI_KEY_ENTER, 0, 0); // the tty is line-buffered
    usleep(300000);
    vui_next_event(ui, &t->node);
    stub_ready_mask = 0;
    node_draw(&t->node, scr);
    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_PRESS, 0, 0, VUI_MOD_SHIFT); // Shift: selects
    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_MOVE, 19, 3, VUI_MOD_SHIFT);
    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_RELEASE, 19, 3, VUI_MOD_SHIFT);
    terminal_selection_text(t, a, sizeof a);
    for (char *p = a, *q = a;; p++) { if (*p != '\n') *q++ = *p; if (!*p) break; } // rows wrap: join them
    assert(strstr(a, "^[[<0;3;2M^[[<0;3;2m^[") != NULL && strstr(a, "<64;3;2M") != NULL); // cat -v shows the escapes: the program got them (cat echoes after the tty's own echo)
    node_destroy(&t->node);

    // soft-wrapped rows copy as one line; double click = word, triple click = the whole wrapped line
    t = terminal_create((const char *const[]){ "sh", "-c", "printf 'alpha beta-gamma delta epsilon zeta eta\\n'; sleep 5", NULL });
    assert(t != NULL);
    t->node.rect = (VuiRect){ 0, 0, 20, 10 };
    node_draw(&t->node, scr);
    usleep(300000);
    stub_ready_mask = 1;
    vui_next_event(ui, &t->node);
    stub_ready_mask = 0;
    node_draw(&t->node, scr);
    first_line(0, 2, a, sizeof a);
    assert(strstr(a, "alpha beta-gamma delta epsilon zeta eta") != NULL);
    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_PRESS, 8, 0, 0);
    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_RELEASE, 8, 0, 0);
    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_PRESS, 8, 0, 0);
    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_RELEASE, 8, 0, 0);
    terminal_selection_text(t, a, sizeof a);
    assert(strcmp(a, "beta-gamma") == 0);
    stub_now += 100;
    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_PRESS, 8, 0, 0);
    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_RELEASE, 8, 0, 0);
    terminal_selection_text(t, a, sizeof a);
    assert(strcmp(a, "alpha beta-gamma delta epsilon zeta eta") == 0);
    stub_now += 1000; // too slow: a fresh single click selects nothing yet
    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_PRESS, 8, 0, 0);
    mouse(VUI_MOUSE_LEFT, VUI_MOUSE_RELEASE, 8, 0, 0);
    assert(terminal_selection_text(t, a, sizeof a) == 0);
    node_destroy(&t->node);
    scr_destroy(scr);
    vui_shutdown(ui);
    puts("terminal: ok");
    return 0;
}
