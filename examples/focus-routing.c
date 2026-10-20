// focus-routing: custom nodes, focus and event bubbling.
//
// Verify:
//   - Tab / Shift-Tab move the highlight across the three boxes (and wrap)
//   - clicking a box focuses it and counts a click
//   - typing counts keys in the focused box only
//   - the bottom bar (not focusable) shows the last event that bubbled up
//     to the root, i.e. what no box handled: Enter, arrows, F-keys...
//   - Ctrl-C quits

#include <stdio.h>

#include "../include/vaporui.h"

// A custom node: VuiNode first, then your own state.
typedef struct {
    VuiNode node;
    const char *name;
    unsigned int keys, clicks;
} Box;

static void box_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    Box *box = (Box *)node;
    VuiStyle style = { .attr = node->focused ? VUI_ATTR_REVERSE : VUI_ATTR_NONE };
    char line[40];

    (void)userdata;
    scr_fill(scr, node->rect, ' ', style);
    scr_puts(scr, node->rect.x + 1, node->rect.y + 1, box->name, style);
    snprintf(line, sizeof(line), "keys: %u", box->keys);
    scr_puts(scr, node->rect.x + 1, node->rect.y + 3, line, style);
    snprintf(line, sizeof(line), "clicks: %u", box->clicks);
    scr_puts(scr, node->rect.x + 1, node->rect.y + 4, line, style);
}

static int box_event(VuiNode *node, VuiCtx *ui, const VuiEvent *event)
{
    Box *box = (Box *)node;

    (void)ui;

    if (event->type == VUI_EVENT_TEXT) {
        box->keys++;
        return 1;
    }

    if (event->type == VUI_EVENT_MOUSE && event->mouse.action == VUI_MOUSE_PRESS) {
        box->clicks++;
        return 1;
    }

    return 0;
}

static Box *box_create(const char *name)
{
    Box *box = (Box *)node_alloc(sizeof(Box));

    box->name = name;
    box->node.grow = 1;
    box->node.focusable = 1;
    box->node.draw = box_draw;
    box->node.on_event = box_event;

    return box;
}

static int root_event(VuiNode *node, VuiCtx *ui, const VuiEvent *event)
{
    (void)ui;

    if (event->type == VUI_EVENT_KEY) {
        snprintf(node->userdata, 64, "bubbled to root: key %d", event->key.key);
        return 1;
    }

    return 0;
}

static void draw_bar(VuiNode *node, VuiScreen *scr, void *userdata)
{
    scr_puts(scr, node->rect.x, node->rect.y, userdata, (VuiStyle){ .attr = VUI_ATTR_BOLD });
}

static void act_quit(VuiCtx *ui, const VuiKeyEvent *event, void *userdata)
{
    (void)ui;
    (void)event;
    *(int *)userdata = 0;
}

int main(void)
{
    char last[64] = "Tab / click to focus, type to count";
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
    root->on_event = root_event;
    root->userdata = last;

    VuiNode *row = node_create();
    row->grow = 1;
    row->gap = 1;
    node_add(root, row);

    node_add(row, &box_create("first")->node);
    node_add(row, &box_create("second")->node);
    node_add(row, &box_create("third")->node);

    VuiNode *bar = node_create();
    bar->basis = 1;
    bar->draw = draw_bar;
    bar->userdata = last;
    node_add(root, bar);

    vui_set_focus(ui, row->children[0]);

    while (running) {
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
