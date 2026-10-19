// Widget logic tests: drive widgets through on_event with synthetic events.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../include/vaporui.h"

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

    puts("widgets: ok");
    return 0;
}
