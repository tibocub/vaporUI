// text-area: multi-line editor with a status line.
//
// Verify:
//   - sample text: a long line that wraps at word boundaries; accents and 日本語 (2 columns each)
//   - arrows, Home/End, Ctrl-Home/End, PageUp/PageDown, Ctrl/Alt+Left/Right; Up/Down follow
//     the visual rows and remember the column across short ones
//   - Shift + any of those selects; dragging with the mouse selects (even past the edges, which
//     scrolls); typing, Backspace or Delete replaces/removes the selection; Alt-A selects all
//   - Ctrl-X cut, Ctrl-C copy (with a selection), Ctrl-V paste; the text also reaches the system
//     clipboard on terminals supporting OSC 52. Terminal paste (Ctrl-Shift-V) inserts too
//   - Ctrl-Z undo, Ctrl-Y redo: a run of typing or Backspace is one step
//   - Enter keeps the line's indentation (try on the indented line), Tab inserts spaces
//   - the "Wrap" button toggles wrapping; without it long lines scroll sideways
//   - the status shows line:column, selection size and character count
//   - Shift-Tab leaves the editor (Tab is the editor's); wheel scrolls; click places the cursor
//   - Ctrl-Q quits

#include <stdio.h>

#include "../include/vaporui.h"

typedef struct {
    int running;
    VuiTextarea *ta;
} App;

static void draw_status(VuiNode *node, VuiScreen *scr, void *userdata)
{
    App *app = userdata;
    unsigned int line, col;
    char buf[96];

    textarea_cursor_pos(app->ta, &line, &col);
    unsigned int sel = app->ta->cursor > app->ta->anchor ? app->ta->cursor - app->ta->anchor : app->ta->anchor - app->ta->cursor;

    snprintf(buf, sizeof(buf), "line %u, col %u   %u selected   %u chars   Ctrl-Q quits", line + 1, col + 1, sel, app->ta->len);
    scr_puts(scr, node->rect.x, node->rect.y, buf, (VuiStyle){ .attr = VUI_ATTR_BOLD });
}

static void on_clear(VuiNode *button)
{
    textarea_set_text(((App *)button->userdata)->ta, "");
}

static void on_wrap(VuiNode *button)
{
    VuiTextarea *ta = ((App *)button->userdata)->ta;

    ta->wrap = !ta->wrap;
    ta->left = 0;
    ta->reveal = 1;
}

static void act_quit(VuiCtx *ui, const VuiKeyEvent *event, void *userdata)
{
    (void)ui;
    (void)event;
    ((App *)userdata)->running = 0;
}

int main(void)
{
    App app = { .running = 1 };
    VuiCtx *ui = vui_init();

    if (ui == NULL) {
        fprintf(stderr, "Failed to initialize VaporUI\n");
        return 1;
    }

    vui_set_mouse(ui, 2);

    VuiControls *global = ctl_create();
    ctl_bind(global, VUI_KEY_CHAR, VUI_MOD_CTRL, 'q', act_quit, &app);

    VuiNode *root = node_create();
    root->direction = VUI_DIR_COLUMN;

    VuiBorder *frame = border_create("text-area");
    frame->node.grow = 1;
    node_add(root, &frame->node);

    app.ta = textarea_create(0);
    app.ta->auto_indent = 1;
    textarea_set_text(app.ta,
        "vaporUI text area\n"
        "\n"
        "This line is deliberately much longer than the editor is wide so that you can check the sideways scrolling and the cursor staying visible.\n"
        "short\n"
        "caf\xc3\xa9 \xe2\x80\x94 \xc3\xa0 \xc3\xa7 \xc3\xb9 \xe2\x82\xac \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\n"
        "    an indented line: press Enter at its end\n"
        "line 6\nline 7\nline 8\nline 9\nline 10\nline 11\nline 12\nline 13\nline 14\nline 15\nline 16\n");
    node_add(&frame->node, &app.ta->node);

    VuiNode *status = node_create();
    status->basis = 1;
    status->draw = draw_status;
    status->userdata = &app;
    node_add(root, status);

    VuiNode *buttons = node_create();
    buttons->basis = 1;
    buttons->gap = 1;
    node_add(root, buttons);

    VuiButton *wrap = button_create("Wrap", on_wrap);
    wrap->node.userdata = &app;
    wrap->node.grow = 1;
    node_add(buttons, &wrap->node);

    VuiButton *clear = button_create("Clear", on_clear);
    clear->node.userdata = &app;
    clear->node.grow = 1;
    node_add(buttons, &clear->node);

    vui_set_focus(ui, &app.ta->node);

    while (app.running) {
        VuiEvent event;

        event = vui_next_event(ui, root);

        if (event.type == VUI_EVENT_CLOSE) {
            break;
        }

        if (event.type == VUI_EVENT_KEY && ctl_dispatch(global, ui, &event.key)) {
            continue;
        }

        vui_dispatch(ui, root, &event);
    }

    node_destroy(root);
    ctl_destroy(global);
    vui_shutdown(ui);

    return 0;
}
