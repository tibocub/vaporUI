// Widget logic tests: drive widgets through on_event with synthetic events.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../include/vaporui.h"
#include "../src/ctx.h"

static int submitted, changes;

static void on_submit(VuiNode *n) { (void)n; submitted++; }
static void on_change(VuiNode *n) { (void)n; changes++; }

static void send(VuiNode *n, VuiEvent e) { n->on_event(n, NULL, &e); }
static void text(VuiNode *n, VuiCodepoint cp) { send(n, (VuiEvent){ .type = VUI_EVENT_TEXT, .text = { cp } }); }
static void key(VuiNode *n, VuiKey k, VuiModifiers m, VuiCodepoint c)
{
    send(n, (VuiEvent){ .type = VUI_EVENT_KEY, .key = { k, m, c } });
}
static void type(VuiNode *n, const char *s) { for (; *s; s++) text(n, (unsigned char)*s); }

static const char *get(VuiInput *in) { static char buf[64]; input_get_text(in, buf, sizeof buf); return buf; }

extern uint64_t stub_now;
extern unsigned int stub_ready_mask;

static int ticks, reads, closed;
static void on_tick(VuiCtx *ui, void *ud) { (void)ui; (void)ud; ticks++; }
static void on_read(VuiCtx *ui, int fd, void *ud) { (void)ud; reads++; vui_unwatch_fd(ui, fd); }
static void on_close(VuiNode *n) { (void)n; closed++; }
static const char *row_text(VuiList *l, unsigned int i) { (void)l; static char b[16]; snprintf(b, sizeof b, "row %u", i); return b; }

static const char *m_a[] = { "a0", "a1" }, *m_b[] = { "b0", "b1", "b2" };
static const VuiMenu test_menus[] = { { "File", m_a, 2 }, { "Edit", m_b, 3 } };
static int picked_menu = -1, picked_item = -1, ctx_item = -1;
static void on_menu(VuiMenubar *b, unsigned int m, unsigned int i) { (void)b; picked_menu = m; picked_item = i; }
static void on_ctx(void *ud, unsigned int i) { (void)ud; ctx_item = i; }

static VuiEvent mouse(VuiMouseButton b, VuiMouseAction a, unsigned int x, unsigned int y)
{
    return (VuiEvent){ .type = VUI_EVENT_MOUSE, .mouse = { b, a, 0, x, y } };
}
static VuiEvent keyev(VuiKey k) { return (VuiEvent){ .type = VUI_EVENT_KEY, .key = { k, 0, 0 } }; }

int main(void)
{
    VuiInput *in = input_create(8);
    VuiNode *n = &in->node;

    in->on_submit = on_submit;
    in->on_change = on_change;

    type(n, "ab");
    text(n, 0xe9);
    assert(!strcmp(get(in), "ab\xc3\xa9") && in->cursor == 3 && changes == 3);

    key(n, VUI_KEY_LEFT, 0, 0);
    key(n, VUI_KEY_BACKSPACE, 0, 0);
    assert(!strcmp(get(in), "a\xc3\xa9") && in->cursor == 1);

    key(n, VUI_KEY_DELETE, 0, 0);
    assert(!strcmp(get(in), "a") && in->cursor == 1);

    text(n, '\n'); // control characters are ignored (pasted newline)
    assert(!strcmp(get(in), "a"));

    type(n, "bcdefghij"); // capacity is 8
    assert(in->len == 8 && !strcmp(get(in), "abcdefgh"));

    key(n, VUI_KEY_HOME, 0, 0);
    assert(in->cursor == 0);
    key(n, VUI_KEY_CHAR, VUI_MOD_CTRL, 'e');
    assert(in->cursor == 8);

    input_set_text(in, "foo bar  baz");
    assert(in->len == 8 && !strcmp(get(in), "foo bar "));
    input_set_text(in, "foo bar baz");
    assert(in->len == 8 && !strcmp(get(in), "foo bar "));

    input_set_text(in, "one two");
    key(n, VUI_KEY_LEFT, VUI_MOD_CTRL, 0);
    assert(in->cursor == 4);
    key(n, VUI_KEY_LEFT, VUI_MOD_ALT, 0);
    assert(in->cursor == 0);
    key(n, VUI_KEY_RIGHT, VUI_MOD_CTRL, 0);
    assert(in->cursor == 3);

    key(n, VUI_KEY_END, 0, 0);
    key(n, VUI_KEY_CHAR, VUI_MOD_CTRL, 'w');
    assert(!strcmp(get(in), "one "));
    key(n, VUI_KEY_BACKSPACE, VUI_MOD_CTRL, 0);
    assert(!strcmp(get(in), ""));

    input_set_text(in, "abcd");
    key(n, VUI_KEY_LEFT, 0, 0);
    key(n, VUI_KEY_LEFT, 0, 0);
    key(n, VUI_KEY_CHAR, VUI_MOD_CTRL, 'k');
    assert(!strcmp(get(in), "ab"));
    key(n, VUI_KEY_CHAR, VUI_MOD_CTRL, 'u');
    assert(!strcmp(get(in), ""));

    key(n, VUI_KEY_ENTER, 0, 0);
    assert(submitted == 1);

    // keys the input doesn't own bubble up
    send(n, (VuiEvent){ .type = VUI_EVENT_KEY, .key = { VUI_KEY_UP, 0, 0 } });
    assert(n->on_event(n, NULL, &(VuiEvent){ .type = VUI_EVENT_KEY, .key = { VUI_KEY_CHAR, VUI_MOD_CTRL, 'c' } }) == 0);

    // output truncates on a character boundary
    input_set_text(in, "\xc3\xa9\xc3\xa9");
    char small[4];
    assert(input_get_text(in, small, sizeof small) == 2 && !strcmp(small, "\xc3\xa9"));
    node_destroy(n);

    // checklist: radio keeps exactly one selection
    const char *items[] = { "a", "b", "c" };
    VuiChecklist *l = checklist_create(items, 3, 1);
    l->node.rect.height = 3;
    key(&l->node, VUI_KEY_DOWN, 0, 0);
    key(&l->node, VUI_KEY_ENTER, 0, 0);
    assert(!l->checked[0] && l->checked[1] && !l->checked[2]);
    node_destroy(&l->node);

    // textarea
    VuiTextarea *ta = textarea_create(0);
    VuiNode *tn = &ta->node;
    char out[128];
    unsigned int line, col;

    tn->rect = (VuiRect){ 0, 0, 20, 4 };
    type(tn, "ab");
    key(tn, VUI_KEY_ENTER, 0, 0);
    type(tn, "cdef");
    key(tn, VUI_KEY_ENTER, 0, 0);
    type(tn, "g");
    textarea_get_text(ta, out, sizeof out);
    assert(!strcmp(out, "ab\ncdef\ng"));
    textarea_cursor_pos(ta, &line, &col);
    assert(line == 2 && col == 1);

    key(tn, VUI_KEY_UP, 0, 0);       // col 1 of "cdef"
    key(tn, VUI_KEY_END, 0, 0);      // col 4
    key(tn, VUI_KEY_DOWN, 0, 0);     // "g" is short: clamp to its end
    textarea_cursor_pos(ta, &line, &col);
    assert(line == 2 && col == 1);
    key(tn, VUI_KEY_UP, 0, 0);       // remembered column is 4 (set by End)
    key(tn, VUI_KEY_UP, 0, 0);       // "ab" is short: clamp
    textarea_cursor_pos(ta, &line, &col);
    assert(line == 0 && col == 2);
    key(tn, VUI_KEY_DOWN, 0, 0);
    textarea_cursor_pos(ta, &line, &col);
    assert(line == 1 && col == 4);

    key(tn, VUI_KEY_HOME, 0, 0);
    key(tn, VUI_KEY_BACKSPACE, 0, 0); // joins "ab" and "cdef"
    textarea_get_text(ta, out, sizeof out);
    assert(!strcmp(out, "abcdef\ng"));

    key(tn, VUI_KEY_CHAR, VUI_MOD_CTRL, 'k'); // kill to end of line
    textarea_get_text(ta, out, sizeof out);
    assert(!strcmp(out, "ab\ng"));

    textarea_set_text(ta, "x\ny");
    key(tn, VUI_KEY_END, VUI_MOD_CTRL, 0);
    textarea_cursor_pos(ta, &line, &col);
    assert(line == 1 && col == 1);
    key(tn, VUI_KEY_TAB, 0, 0);       // to next multiple of 4
    textarea_get_text(ta, out, sizeof out);
    assert(!strcmp(out, "x\ny   "));
    assert(tn->on_event(tn, NULL, &(VuiEvent){ .type = VUI_EVENT_KEY, .key = { VUI_KEY_TAB, VUI_MOD_SHIFT, 0 } }) == 0);

    textarea_set_text(ta, "one\ntwo\n");
    key(tn, VUI_KEY_END, VUI_MOD_CTRL, 0); // trailing empty line
    textarea_cursor_pos(ta, &line, &col);
    assert(line == 2 && col == 0);
    text(tn, '\n'); // pasted newline inserts
    textarea_get_text(ta, out, sizeof out);
    assert(!strcmp(out, "one\ntwo\n\n"));
    node_destroy(tn);

    VuiTextarea *capped = textarea_create(3);
    type(&capped->node, "abcdef");
    assert(capped->len == 3);
    node_destroy(&capped->node);

    // selection, clipboard
    VuiTextarea *sa = textarea_create(0);
    VuiNode *sn = &sa->node;
    sn->rect = (VuiRect){ 0, 0, 20, 4 };
    textarea_set_text(sa, "hello world");
    key(sn, VUI_KEY_RIGHT, VUI_MOD_SHIFT, 0);
    key(sn, VUI_KEY_RIGHT, VUI_MOD_SHIFT, 0);
    textarea_get_selection(sa, out, sizeof out);
    assert(!strcmp(out, "he"));
    key(sn, VUI_KEY_CHAR, VUI_MOD_CTRL, 'x');  // cut
    textarea_get_text(sa, out, sizeof out);
    assert(!strcmp(out, "llo world") && !strcmp(vui_clipboard_get(), "he"));
    key(sn, VUI_KEY_END, 0, 0);
    key(sn, VUI_KEY_CHAR, VUI_MOD_CTRL, 'v');  // paste at the end
    textarea_get_text(sa, out, sizeof out);
    assert(!strcmp(out, "llo worldhe"));
    key(sn, VUI_KEY_LEFT, VUI_MOD_CTRL | VUI_MOD_SHIFT, 0); // select the last word
    text(sn, 'X');                                          // typing replaces the selection
    textarea_get_text(sa, out, sizeof out);
    assert(!strcmp(out, "llo X"));
    assert(sn->on_event(sn, NULL, &(VuiEvent){ .type = VUI_EVENT_KEY, .key = { VUI_KEY_CHAR, VUI_MOD_CTRL, 'c' } }) == 0); // no selection: bubbles
    textarea_select_all(sa);
    key(sn, VUI_KEY_BACKSPACE, 0, 0);
    assert(sa->len == 0);

    // undo / redo: typing merges into one step, backspace runs too
    textarea_set_text(sa, "");
    type(sn, "abc");
    key(sn, VUI_KEY_ENTER, 0, 0);
    type(sn, "def");
    key(sn, VUI_KEY_BACKSPACE, 0, 0);
    key(sn, VUI_KEY_BACKSPACE, 0, 0);
    textarea_get_text(sa, out, sizeof out);
    assert(!strcmp(out, "abc\nd"));
    key(sn, VUI_KEY_CHAR, VUI_MOD_CTRL, 'z');   // undoes both backspaces
    textarea_get_text(sa, out, sizeof out);
    assert(!strcmp(out, "abc\ndef") && sa->cursor == 7);
    key(sn, VUI_KEY_CHAR, VUI_MOD_CTRL, 'z');   // typing "def"
    key(sn, VUI_KEY_CHAR, VUI_MOD_CTRL, 'z');   // the newline
    textarea_get_text(sa, out, sizeof out);
    assert(!strcmp(out, "abc"));
    key(sn, VUI_KEY_CHAR, VUI_MOD_CTRL, 'y');
    key(sn, VUI_KEY_CHAR, VUI_MOD_CTRL, 'y');
    textarea_get_text(sa, out, sizeof out);
    assert(!strcmp(out, "abc\ndef"));
    type(sn, "!");                              // new edit drops the redo history
    key(sn, VUI_KEY_CHAR, VUI_MOD_CTRL, 'y');
    textarea_get_text(sa, out, sizeof out);
    assert(!strcmp(out, "abc\ndef!"));
    textarea_select_all(sa);
    type(sn, "z");
    key(sn, VUI_KEY_CHAR, VUI_MOD_CTRL, 'z');   // undo a replaced selection restores it
    textarea_get_text(sa, out, sizeof out);
    assert(!strcmp(out, "abc\ndef!"));

    // Shift + Home/End/PageUp/PageDown select too
    sn->rect = (VuiRect){ 0, 0, 20, 3 };
    textarea_set_text(sa, "l1\nl2\nl3\nl4\nl5\nl6");
    key(sn, VUI_KEY_END, VUI_MOD_SHIFT, 0);
    assert(sa->cursor == 2 && sa->anchor == 0);
    key(sn, VUI_KEY_HOME, VUI_MOD_SHIFT, 0);
    assert(sa->cursor == 0 && sa->anchor == 0);
    key(sn, VUI_KEY_PAGE_DOWN, VUI_MOD_SHIFT, 0);
    assert(sa->anchor == 0 && sa->cursor == 6);   // 2 rows down (height 3 - 1)
    key(sn, VUI_KEY_PAGE_UP, VUI_MOD_SHIFT, 0);
    assert(sa->anchor == 0 && sa->cursor == 0);
    key(sn, VUI_KEY_END, VUI_MOD_CTRL | VUI_MOD_SHIFT, 0);
    assert(sa->anchor == 0 && sa->cursor == sa->len);

    // auto-indent
    sa->auto_indent = 1;
    textarea_set_text(sa, "    foo");
    key(sn, VUI_KEY_END, VUI_MOD_CTRL, 0);
    key(sn, VUI_KEY_ENTER, 0, 0);
    type(sn, "x");
    textarea_get_text(sa, out, sizeof out);
    assert(!strcmp(out, "    foo\n    x"));

    // wrapping: width 8 leaves 7 columns per row, breaking after spaces
    sn->rect = (VuiRect){ 0, 0, 8, 4 };
    textarea_set_text(sa, "aaa bbb ccc");      // rows: "aaa bbb " and "ccc"
    key(sn, VUI_KEY_DOWN, 0, 0);
    assert(sa->cursor == 8);
    key(sn, VUI_KEY_DOWN, 0, 0);               // last row: stays
    assert(sa->cursor == 8);
    key(sn, VUI_KEY_HOME, 0, 0);
    assert(sa->cursor == 8);
    key(sn, VUI_KEY_UP, 0, 0);
    assert(sa->cursor == 0);
    key(sn, VUI_KEY_END, 0, 0);                // end of a wrapped row is before the break
    assert(sa->cursor == 7);
    sa->wrap = 0;
    textarea_set_text(sa, "aaa bbb ccc");
    key(sn, VUI_KEY_DOWN, 0, 0);
    assert(sa->cursor == 0);                   // single row without wrapping
    sa->wrap = 1;

    // wide characters: Up/Down use display columns (each CJK character is 2 wide)
    sn->rect = (VuiRect){ 0, 0, 20, 4 };
    textarea_set_text(sa, "\xe6\x97\xa5\xe6\x9c\xac\nabcd");
    key(sn, VUI_KEY_RIGHT, 0, 0);
    key(sn, VUI_KEY_RIGHT, 0, 0);              // after both characters: column 4
    key(sn, VUI_KEY_DOWN, 0, 0);
    assert(sa->cursor == 3 + 4);
    assert(vui_cp_width(0x65E5) == 2 && vui_cp_width('a') == 1 && vui_cp_width(0x301) == 0 && vui_str_width("a\xe6\x97\xa5") == 3);

    // max_chars with a selection; history survives set_text
    textarea_set_text(sa, "abc");
    sa->max = 3;
    textarea_select_all(sa);
    type(sn, "xyz123");
    textarea_get_text(sa, out, sizeof out);
    assert(!strcmp(out, "xyz") && sa->cursor == 3);
    node_destroy(sn);

    // content-based sizing: buttons are label + 4 wide, 2-cell gap, leftover stays empty
    VuiNode *row = node_create();
    row->gap = 2;
    VuiButton *b1 = button_create("OK", NULL), *b2 = button_create("Cancel", NULL);
    VuiLabel *lab = label_create("ab\nlonger");
    node_add(row, &b1->node);
    node_add(row, &b2->node);
    node_layout(row, (VuiRect){ 0, 0, 40, 1 });
    assert(b1->node.rect.x == 0 && b1->node.rect.width == 6);
    assert(b2->node.rect.x == 8 && b2->node.rect.width == 10);
    assert(node_measure(row, 1) == 6 + 2 + 10 && node_measure(row, 0) == 1);
    assert(node_measure(&lab->node, 1) == 6 && node_measure(&lab->node, 0) == 2);
    node_destroy(&lab->node);
    node_destroy(row);

    // justify / align / per-side padding
    row = node_create();
    b1 = button_create("OK", NULL); b2 = button_create("Cancel", NULL);
    node_add(row, &b1->node);
    node_add(row, &b2->node);
    row->justify = VUI_JUSTIFY_END;
    node_layout(row, (VuiRect){ 0, 0, 40, 3 });
    assert(b1->node.rect.x == 24 && b2->node.rect.x == 30);
    row->justify = VUI_JUSTIFY_CENTER;
    node_layout(row, (VuiRect){ 0, 0, 40, 3 });
    assert(b1->node.rect.x == 12);
    row->justify = VUI_JUSTIFY_BETWEEN;
    node_layout(row, (VuiRect){ 0, 0, 40, 3 });
    assert(b1->node.rect.x == 0 && b2->node.rect.x + b2->node.rect.width == 40);
    row->justify = VUI_JUSTIFY_START;
    assert(b1->node.rect.height == 3);
    row->align = VUI_ALIGN_CENTER;
    node_pad(row, 1, 2, 0, 3);
    node_layout(row, (VuiRect){ 0, 0, 40, 5 });
    assert(b1->node.rect.x == 3 && b1->node.rect.y == 1 + (4 - 1) / 2 && b1->node.rect.height == 1);
    node_destroy(row);

    // scrolling container: children keep their size, scroll shifts them and is clamped
    VuiNode *sc = node_create();
    sc->direction = VUI_DIR_COLUMN;
    sc->scrolls = 1;
    for (int i = 0; i < 10; i++) { VuiNode *c = node_create(); c->basis = 2; node_add(sc, c); }
    node_layout(sc, (VuiRect){ 0, 0, 10, 6 });
    assert(sc->content == 20 && sc->children[9]->rect.y == 18 && sc->children[9]->rect.height == 2);
    sc->scroll = 100;
    node_layout(sc, (VuiRect){ 0, 0, 10, 6 });
    assert(sc->scroll == 14 && sc->children[9]->rect.y == 4 && sc->children[0]->rect.y == -14);
    node_destroy(sc);

    // timers and fd watches
    VuiCtx *ui = vui_init();
    assert(ui != NULL && vui_ctx() == ui);
    int repeating = vui_timer(ui, 100, 1, on_tick, NULL), once = vui_timer(ui, 250, 0, on_tick, NULL);
    assert(repeating && once && repeating != once);
    stub_now = 50;
    assert(vui_poll_event(ui).type == VUI_EVENT_NONE && ticks == 0);
    stub_now = 100;
    assert(vui_poll_event(ui).type == VUI_EVENT_WAKE && ticks == 1);
    stub_now = 300;
    vui_poll_event(ui);
    assert(ticks == 3); // repeating (again) + one-shot
    vui_timer_cancel(ui, repeating);
    stub_now = 1000;
    assert(vui_poll_event(ui).type == VUI_EVENT_NONE && ticks == 3);
    assert(vui_watch_fd(ui, 7, on_read, NULL) == 0);
    stub_ready_mask = 1;
    assert(vui_poll_event(ui).type == VUI_EVENT_WAKE && reads == 1); // callback unwatched itself
    assert(vui_poll_event(ui).type == VUI_EVENT_NONE && reads == 1);
    stub_ready_mask = 0;

    // overlays: modal focus scope, dismiss, focus restore, raise, window drag
    VuiNode *root = node_create();
    VuiButton *under = button_create("under", NULL);
    node_add(root, &under->node);
    vui_set_focus(ui, &under->node);

    VuiNode *dialog = node_create();
    VuiButton *d1 = button_create("one", NULL), *d2 = button_create("two", NULL);
    dialog->direction = VUI_DIR_COLUMN;
    node_add(dialog, &d1->node);
    node_add(dialog, &d2->node);
    assert(vui_overlay_add(ui, dialog, (VuiRect){ 77, 23, 0, 0 }, VUI_OVERLAY_MODAL | VUI_OVERLAY_DISMISS) == 0);
    assert(dialog->rect.width == 7 && dialog->rect.x == 73 && dialog->rect.y == 22); // measured, pushed back on screen
    assert(vui_focus(ui) == &d1->node);
    VuiEvent tab = keyev(VUI_KEY_TAB);
    vui_dispatch(ui, root, &tab);
    assert(vui_focus(ui) == &d2->node);
    vui_dispatch(ui, root, &tab);
    assert(vui_focus(ui) == &d1->node); // wraps inside the modal, never reaches `under`
    VuiEvent esc = keyev(VUI_KEY_ESCAPE);
    assert(vui_dispatch(ui, root, &esc) == 1 && ui->overlay_count == 0);
    assert(vui_focus(ui) == &under->node);
    vui_dispatch(ui, root, &(VuiEvent){ .type = VUI_EVENT_NONE }); // frees the removed overlay

    dialog = node_create();
    VuiButton *d3 = button_create("x", NULL);
    node_add(dialog, &d3->node);
    vui_overlay_add(ui, dialog, (VuiRect){ 10, 10, 5, 1 }, VUI_OVERLAY_DISMISS);
    VuiEvent click = mouse(VUI_MOUSE_LEFT, VUI_MOUSE_PRESS, 0, 0);
    assert(vui_dispatch(ui, root, &click) == 1 && ui->overlay_count == 0); // click outside dismisses and is consumed
    vui_dispatch(ui, root, &(VuiEvent){ .type = VUI_EVENT_NONE });

    VuiWindow *w1 = window_create("one"), *w2 = window_create("two");
    w1->on_close = on_close;
    vui_overlay_add(ui, &w1->node, (VuiRect){ 5, 5, 20, 8 }, 0);
    vui_overlay_add(ui, &w2->node, (VuiRect){ 10, 8, 20, 8 }, 0);
    assert(ui->overlays[1].node == &w2->node);
    click = mouse(VUI_MOUSE_LEFT, VUI_MOUSE_PRESS, 6, 6); // only w1 is under (6,6)
    vui_dispatch(ui, root, &click);
    assert(ui->overlays[1].node == &w1->node); // raised
    click = mouse(VUI_MOUSE_LEFT, VUI_MOUSE_PRESS, 8, 5); // title bar of w1
    vui_dispatch(ui, root, &click);
    VuiEvent drag = mouse(VUI_MOUSE_LEFT, VUI_MOUSE_MOVE, 18, 9);
    vui_dispatch(ui, root, &drag);
    assert(w1->node.rect.x == 15 && w1->node.rect.y == 9);
    VuiEvent up = mouse(VUI_MOUSE_LEFT, VUI_MOUSE_RELEASE, 18, 9);
    vui_dispatch(ui, root, &up);
    click = mouse(VUI_MOUSE_LEFT, VUI_MOUSE_PRESS, 15 + 19, 9 + 7); // bottom-right corner of w1
    vui_dispatch(ui, root, &click);
    drag = mouse(VUI_MOUSE_LEFT, VUI_MOUSE_MOVE, 40, 20);
    vui_dispatch(ui, root, &drag);
    assert(w1->node.rect.width == 26 && w1->node.rect.height == 12);
    up = mouse(VUI_MOUSE_LEFT, VUI_MOUSE_RELEASE, 40, 20);
    vui_dispatch(ui, root, &up);
    click = mouse(VUI_MOUSE_LEFT, VUI_MOUSE_PRESS, 15 + 25 - 1, 9); // [x] of w1
    vui_dispatch(ui, root, &click);
    assert(closed == 1 && ui->overlay_count == 2);
    vui_overlay_remove(ui, &w1->node);
    vui_overlay_remove(ui, &w2->node);
    assert(ui->overlay_count == 0);

    // virtual list
    VuiList *list = list_create(100, row_text);
    list->node.rect = (VuiRect){ 0, 0, 10, 5 };
    vui_set_focus(ui, &list->node);
    for (int i = 0; i < 7; i++) { VuiEvent down = keyev(VUI_KEY_DOWN); list->node.on_event(&list->node, ui, &down); }
    assert(list->cursor == 7 && list->follow);
    VuiEvent end = keyev(VUI_KEY_END), pgup = keyev(VUI_KEY_PAGE_UP);
    list->node.on_event(&list->node, ui, &end);
    assert(list->cursor == 99);
    list->node.on_event(&list->node, ui, &pgup);
    assert(list->cursor == 95);
    list_set_count(list, 10);
    assert(list->cursor == 9 && list->follow);
    node_destroy(&list->node);

    // scroll container: focusing a child scrolls it into view
    root->direction = VUI_DIR_COLUMN;
    ui->root = NULL; ui->focus = NULL; // forget what we are about to free
    node_destroy(root);
    root = scroll_create();
    VuiButton *btns[6];
    for (int i = 0; i < 6; i++) { btns[i] = button_create("b", NULL); btns[i]->node.basis = 3; node_add(root, &btns[i]->node); }
    node_layout(root, (VuiRect){ 0, 0, 10, 6 });
    assert(root->content == 18 && root->scroll == 0);
    vui_set_focus(ui, &btns[3]->node);
    node_layout(root, (VuiRect){ 0, 0, 10, 6 });
    assert(root->scroll == 6 && btns[3]->node.rect.y == 3);
    vui_set_focus(ui, &btns[0]->node);
    node_layout(root, (VuiRect){ 0, 0, 10, 6 });
    assert(root->scroll == 0);
    node_destroy(root);

    VuiMenubar *mb = menubar_create(test_menus, 2, on_menu);
    root = node_create();
    node_add(root, &mb->node);
    node_layout(root, (VuiRect){ 0, 0, 40, 10 });
    vui_set_focus(ui, &mb->node);
    VuiEvent down = keyev(VUI_KEY_DOWN), right = keyev(VUI_KEY_RIGHT), enter = keyev(VUI_KEY_ENTER);
    vui_dispatch(ui, root, &down);
    assert(ui->overlay_count == 1 && ui->overlays[0].node->rect.x == 0 && ui->overlays[0].node->rect.y == 1);
    vui_dispatch(ui, root, &right); // open menu jumps to the neighbour
    assert(ui->overlay_count == 1 && mb->active == 1 && ui->overlays[0].node->rect.x == 6);
    vui_dispatch(ui, root, &enter);
    assert(picked_menu == 1 && picked_item == 0 && ui->overlay_count == 0);
    vui_dispatch(ui, root, &(VuiEvent){ .type = VUI_EVENT_NONE });

    menu_popup(30, 8, m_b, 3, on_ctx, NULL);
    assert(ui->overlay_count == 1 && ui->overlays[0].node->rect.y + (int)ui->overlays[0].node->rect.height <= 24); // clamped on screen
    vui_dispatch(ui, root, &down);
    vui_dispatch(ui, root, &enter);
    assert(ctx_item == 1 && ui->overlay_count == 0);
    vui_dispatch(ui, root, &(VuiEvent){ .type = VUI_EVENT_NONE });
    node_destroy(root);

    root = node_create();
    VuiTextarea *ta2 = textarea_create(0);
    VuiButton *after = button_create("after", NULL);
    node_add(root, &ta2->node);
    node_add(root, &after->node);
    vui_set_focus(ui, &ta2->node);
    VuiEvent f6 = keyev(VUI_KEY_F6), tabkey = keyev(VUI_KEY_TAB);
    vui_dispatch(ui, root, &tabkey);
    assert(vui_focus(ui) == &ta2->node); // Tab belongs to the text area
    vui_dispatch(ui, root, &f6);
    assert(vui_focus(ui) == &after->node); // F6 always moves on
    node_destroy(root);
    vui_shutdown(ui);

    puts("widgets: ok");
    return 0;
}
