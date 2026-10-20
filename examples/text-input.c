// text-input: single-line input fields in a small form.
//
// Verify:
//   - Tab / Shift-Tab / click move between "name", "search" and the Clear button
//   - type text incl. accents (é à ç) and 日本語; Backspace/Delete/arrows/Home/End work
//   - Ctrl-A/E jump to line ends, Ctrl/Alt+Left/Right jump words, Ctrl-W deletes
//     a word back, Ctrl-U / Ctrl-K delete to start / end
//   - clicking inside a field puts the cursor there
//   - text longer than the field scrolls sideways and the cursor stays visible
//   - "name" holds at most 12 characters; "search" is wider
//   - pasting a multi-line text inserts it on one line (newlines dropped)
//   - Enter in "name" shows the greeting; the status line counts "search" characters
//   - Clear empties both fields; Ctrl-C quits

#include <stdio.h>
#include <string.h>

#include "../include/vaporui.h"

typedef struct {
    int running;
    VuiInput *name, *search;
    char greeting[96];
    unsigned int search_len;
} App;

static void draw_status(VuiNode *node, VuiScreen *scr, void *userdata)
{
    App *app = userdata;
    char line[128];

    snprintf(line, sizeof(line), "%s   [search: %u chars]", app->greeting, app->search_len);
    scr_puts(scr, node->rect.x, node->rect.y, line, (VuiStyle){ .attr = VUI_ATTR_BOLD });
}

static void on_submit(VuiNode *input)
{
    App *app = input->userdata;
    char name[64];

    input_get_text(app->name, name, sizeof(name));
    snprintf(app->greeting, sizeof(app->greeting), "Hello, %s!", name);
}

static void on_search(VuiNode *input)
{
    App *app = input->userdata;

    app->search_len = app->search->len;
}

static void on_clear(VuiNode *button)
{
    App *app = button->userdata;

    input_set_text(app->name, "");
    input_set_text(app->search, "");
    app->search_len = 0;
    app->greeting[0] = '\0';
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

    VuiBorder *frame = border_create("form");
    frame->node.direction = VUI_DIR_COLUMN;
    frame->node.basis = 7;
    node_add(root, &frame->node);

    app.name = input_create(12);
    app.name->node.userdata = &app;
    app.name->on_submit = on_submit;
    node_add(&frame->node, &label_create("name (Enter to greet, max 12):")->node);
    node_add(&frame->node, &app.name->node);

    app.search = input_create(200);
    app.search->node.userdata = &app;
    app.search->on_change = on_search;
    node_add(&frame->node, &label_create("search:")->node);
    node_add(&frame->node, &app.search->node);

    VuiButton *clear = button_create("Clear", on_clear);
    clear->node.userdata = &app;
    node_add(&frame->node, &clear->node);

    VuiNode *status = node_create();
    status->basis = 1;
    status->draw = draw_status;
    status->userdata = &app;
    node_add(root, status);

    vui_set_focus(ui, &app.name->node);

    while (app.running) {
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
