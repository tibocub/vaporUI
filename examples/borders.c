// borders: border and div containers.
//
// Verify:
//   - three frames side by side: single, rounded, double, each with its title
//   - the long title on the narrow frame is cut before the corner, corner intact
//   - the middle frame holds a nested frame (nesting shrinks inward by 1 cell)
//   - the right frame's div paints a blue background that stays inside the frame
//   - shrink the terminal: frames stay intact, nothing draws outside its frame
//   - Ctrl-C quits

#include <stdio.h>

#include "../include/vaporui.h"

static void draw_text(VuiNode *node, VuiScreen *scr, void *userdata)
{
    scr_puts(scr, node->rect.x, node->rect.y, userdata, (VuiStyle){0});
}

static VuiNode *text(const char *s)
{
    VuiNode *n = node_create();

    n->draw = draw_text;
    n->userdata = (void *)s;
    return n;
}

int main(void)
{
    VuiCtx *ui = vui_init();

    if (ui == NULL) {
        fprintf(stderr, "Failed to initialize VaporUI\n");
        return 1;
    }

    VuiNode *root = node_create();
    root->gap = 1;

    VuiBorder *single = border_create("single");
    single->node.grow = 1;
    node_add(&single->node, text("plain frame"));
    node_add(root, &single->node);

    VuiBorder *rounded = border_create("rounded, with a very long title");
    rounded->border = VUI_BORDER_ROUNDED;
    rounded->style.fg = VUI_COLOR_CYAN;
    rounded->node.grow = 1;

    VuiBorder *inner = border_create("nested");
    inner->border = VUI_BORDER_DOUBLE;
    inner->node.grow = 1;
    node_add(&inner->node, text("inside"));
    node_add(&rounded->node, &inner->node);
    node_add(root, &rounded->node);

    VuiBorder *dbl = border_create("div");
    dbl->border = VUI_BORDER_DOUBLE;
    dbl->style.fg = VUI_COLOR_YELLOW;
    dbl->node.grow = 1;

    VuiDiv *div = div_create(VUI_DIR_COLUMN);
    div->style.bg = VUI_COLOR_BLUE;
    div->style.fg = VUI_COLOR_WHITE;
    div->node.grow = 1;
    node_add(&div->node, text("on blue"));
    node_add(&dbl->node, &div->node);
    node_add(root, &dbl->node);

    for (;;) {
        VuiEvent event;

        event = vui_next_event(ui, root);

        if (event.type == VUI_EVENT_CLOSE) {
            break;
        }

        if (event.type == VUI_EVENT_KEY && event.key.key == VUI_KEY_CHAR
            && event.key.modifiers == VUI_MOD_CTRL && event.key.character == 'c') {
            break;
        }
    }

    node_destroy(root);
    vui_shutdown(ui);

    return 0;
}
