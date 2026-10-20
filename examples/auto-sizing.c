// auto-sizing: widgets size themselves from their content; no basis numbers anywhere.
//
// Verify:
//   - the title label is one row high; the button row is one row high, each button exactly as
//     wide as its label plus brackets, with a 2-cell gap, packed to the left
//   - the framed pane is as wide and tall as its content (a 3-line label and a checklist
//     of 3 items), not stretched: its right edge hugs the longest line
//   - the bottom input is 1 row high; the wide filler pane takes everything that is left (grow)
//   - resizing the terminal keeps the auto-sized parts the same, only the filler changes
//   - Tab moves between buttons, list and input; Ctrl-C quits

#include <stdio.h>

#include "../include/vaporui.h"

static const char *items[] = { "red", "green", "a much longer blue" };

static void act_quit(VuiCtx *ui, const VuiKeyEvent *event, void *userdata)
{
    (void)ui;
    (void)event;
    *(int *)userdata = 0;
}

static void on_quit(VuiNode *button)
{
    *(int *)button->userdata = 0;
}

int main(void)
{
    int running = 1;
    VuiCtx *ui = vui_init();

    if (ui == NULL) {
        fprintf(stderr, "Failed to initialize VaporUI\n");
        return 1;
    }

    vui_set_mouse(ui, 2);

    VuiControls *global = ctl_create();
    ctl_bind(global, VUI_KEY_CHAR, VUI_MOD_CTRL, 'c', act_quit, &running);

    VuiNode *root = node_create();
    root->direction = VUI_DIR_COLUMN;
    root->gap = 1;

    VuiLabel *title = label_create("auto-sizing");
    title->style.attr = VUI_ATTR_BOLD;
    node_add(root, &title->node);

    VuiNode *buttons = node_create();
    buttons->basis = VUI_AUTO; // a container measures its children
    buttons->gap = 2;
    node_add(root, buttons);

    VuiButton *ok = button_create("OK", NULL);
    VuiButton *cancel = button_create("Cancel", NULL);
    VuiButton *quit = button_create("Quit", on_quit);
    quit->node.userdata = &running;
    node_add(buttons, &ok->node);
    node_add(buttons, &cancel->node);
    node_add(buttons, &quit->node);

    VuiNode *middle = node_create();
    middle->grow = 1;
    node_add(root, middle);

    VuiBorder *frame = border_create("auto");
    frame->node.basis = VUI_AUTO;
    frame->node.direction = VUI_DIR_COLUMN;
    node_add(middle, &frame->node);

    VuiLabel *text = label_create("three lines\nof static text\nin a label");
    node_add(&frame->node, &text->node);

    VuiChecklist *list = checklist_create(items, 3, 0);
    list->node.grow = 0; // exactly its items, not the leftover height
    node_add(&frame->node, &list->node);

    VuiBorder *filler = border_create("grows");
    filler->node.grow = 1;
    node_add(middle, &filler->node);

    VuiInput *input = input_create(24);
    node_add(root, &input->node);

    vui_set_focus(ui, &ok->node);

    while (running) {
        VuiEvent event;

        vui_render(ui, root);

        do {
            event = vui_poll_event(ui);
        } while (event.type == VUI_EVENT_NONE);

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
