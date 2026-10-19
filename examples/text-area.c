// text-area: multi-line editor with a status line.
//
// Verify:
//   - the editor starts with sample text: a long line scrolls sideways, many lines scroll vertically
//   - arrows, Home/End, Ctrl-Home/End, PageUp/PageDown, Ctrl/Alt+Left/Right move; Up/Down
//     remember the column across short lines
//   - Enter splits a line, Backspace at a line start joins, Tab inserts spaces, Ctrl-K / Ctrl-U / Ctrl-W
//   - paste a multi-line text: line breaks are kept
//   - wheel scrolls, click places the cursor
//   - the status shows line:column and the character count live
//   - Shift-Tab leaves the editor (Tab is the editor's); Ctrl-Tab also, on terminals that send it;
//     the Clear button (Enter/Space/click) empties the editor
//   - Ctrl-C quits

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
    snprintf(buf, sizeof(buf), "line %u, col %u   %u chars   (Shift-Tab leaves the editor)", line + 1, col + 1, app->ta->len);
    scr_puts(scr, node->rect.x, node->rect.y, buf, (VuiStyle){ .attr = VUI_ATTR_BOLD });
}

static void on_clear(VuiNode *button)
{
    textarea_set_text(((App *)button->userdata)->ta, "");
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
    ctl_bind(global, VUI_KEY_CHAR, VUI_MOD_CTRL, 'c', act_quit, &app);

    VuiNode *root = node_create();
    root->direction = VUI_DIR_COLUMN;

    VuiBorder *frame = border_create("text-area");
    frame->node.grow = 1;
    node_add(root, &frame->node);

    app.ta = textarea_create(0);
    textarea_set_text(app.ta,
        "vaporUI text area\n"
        "\n"
        "This line is deliberately much longer than the editor is wide so that you can check the sideways scrolling and the cursor staying visible.\n"
        "short\n"
        "caf\xc3\xa9 \xe2\x80\x94 \xc3\xa0 \xc3\xa7 \xc3\xb9 \xe2\x82\xac\n"
        "line 6\nline 7\nline 8\nline 9\nline 10\nline 11\nline 12\nline 13\nline 14\nline 15\nline 16\n");
    node_add(&frame->node, &app.ta->node);

    VuiNode *status = node_create();
    status->basis = 1;
    status->draw = draw_status;
    status->userdata = &app;
    node_add(root, status);

    VuiButton *clear = button_create("Clear", on_clear);
    clear->node.userdata = &app;
    node_add(root, &clear->node);

    vui_set_focus(ui, &app.ta->node);

    while (app.running) {
        VuiEvent event;

        vui_render(ui, root);

        do {
            event = vui_poll_event(ui);
        } while (event.type == VUI_EVENT_NONE);

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
