// buttons-and-lists: button, checklist, radio list, borders, focus.
//
// Verify:
//   - Tab / Shift-Tab cycle: checklist -> radio list -> Add -> Reset -> Quit
//   - Up/Down/Home/End move the cursor, Space/Enter/click toggle; the radio
//     list always has exactly one selection
//   - the checklist has more items than rows: it scrolls to follow the cursor
//   - "Add" bumps the counter, "Reset" zeroes it (Enter, Space or click)
//   - the status line reports the last change
//   - hovering a button or a list row highlights it (blue); the whole button
//     rect is filled, so what lights up is what a click activates
//   - "Quit" or Ctrl-C exits

#include <stdio.h>

#include "../include/vaporui.h"

static const char *toppings[] = { "cheese", "ham", "olives", "mushrooms", "peppers", "onion", "basil", "anchovies" };
static const char *sizes[] = { "small", "medium", "large" };

typedef struct {
    int running;
    int count;
    char status[64];
} App;

static void draw_status(VuiNode *node, VuiScreen *scr, void *userdata)
{
    App *app = userdata;
    char line[96];

    snprintf(line, sizeof(line), "count: %d   %s", app->count, app->status);
    scr_puts(scr, node->rect.x, node->rect.y, line, (VuiStyle){ .attr = VUI_ATTR_BOLD });
}

static void on_add(VuiNode *button)
{
    App *app = button->userdata;

    app->count++;
    snprintf(app->status, sizeof(app->status), "added");
}

static void on_reset(VuiNode *button)
{
    App *app = button->userdata;

    app->count = 0;
    snprintf(app->status, sizeof(app->status), "reset");
}

static void on_quit(VuiNode *button)
{
    ((App *)button->userdata)->running = 0;
}

static void on_topping(VuiNode *list, unsigned int i)
{
    App *app = list->userdata;

    snprintf(app->status, sizeof(app->status), "%s: %s", toppings[i], ((VuiChecklist *)list)->checked[i] ? "on" : "off");
}

static void on_size(VuiNode *list, unsigned int i)
{
    snprintf(((App *)list->userdata)->status, 64, "size: %s", sizes[i]);
}

static VuiBorder *framed(VuiNode *parent, const char *title, VuiNode *content, unsigned int basis)
{
    VuiBorder *b = border_create(title);

    b->node.basis = basis;
    node_add(&b->node, content);
    node_add(parent, &b->node);
    return b;
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

    VuiNode *row = node_create();
    row->grow = 1;
    node_add(root, row);

    VuiChecklist *top = checklist_create(toppings, 8, 0);
    top->node.userdata = &app;
    top->on_change = on_topping;
    framed(row, "toppings", &top->node, 24)->node.basis = 24;

    VuiChecklist *size = checklist_create(sizes, 3, 1);
    size->node.userdata = &app;
    size->on_change = on_size;
    framed(row, "size", &size->node, 18);

    VuiNode *buttons = node_create();
    buttons->direction = VUI_DIR_COLUMN;
    buttons->grow = 1;
    buttons->gap = 1;

    VuiButton *add = button_create("Add", on_add);
    add->node.userdata = &app;
    node_add(buttons, &add->node);

    VuiButton *reset = button_create("Reset", on_reset);
    reset->node.userdata = &app;
    node_add(buttons, &reset->node);

    VuiButton *quit = button_create("Quit", on_quit);
    quit->node.userdata = &app;
    node_add(buttons, &quit->node);

    framed(row, "actions", buttons, 0)->node.grow = 1;

    VuiNode *status = node_create();
    status->basis = 1;
    status->draw = draw_status;
    status->userdata = &app;
    node_add(root, status);

    vui_set_focus(ui, &top->node);

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
