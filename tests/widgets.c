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

    puts("widgets: ok");
    return 0;
}
