// layout-resize: the flexbox subset on a real terminal.
//
// Verify:
//   - header (3 rows) and footer (1 row) keep their height; the body takes the rest
//   - body row: left pane fixed at 16 cols, middle grows 2x, right grows 1x
//   - middle column: 2 stacked panes sharing the height, with a 1-cell gap and padding
//   - each pane prints its own computed rect; no text spills outside its colored box
//   - resize the terminal (drag the window, or shrink it very small): layout
//     recomputes, nothing crashes, nothing is left behind from the old size
//   - q or Ctrl-C quits

#include <stdio.h>

#include "../include/vaporui.h"

typedef struct {
    const char *name;
    VuiColor color;
} Pane;

static void draw_pane(VuiNode *node, VuiScreen *scr, void *userdata)
{
    const Pane *pane = userdata;
    VuiRect r = node->rect;
    VuiStyle style = { .fg = VUI_COLOR_BLACK, .bg = pane->color };
    char line[48];

    for (unsigned int y = 0; y < r.height; y++) {
        for (unsigned int x = 0; x < r.width; x++) {
            scr_set(scr, r.x + x, r.y + y, ' ', style);
        }
    }

    snprintf(line, sizeof(line), "%s %ux%u @%d,%d", pane->name, r.width, r.height, r.x, r.y);

    // draw callbacks aren't clipped yet: truncate by hand
    for (unsigned int i = 0; line[i] != '\0' && i < r.width && r.height > 0; i++) {
        scr_set(scr, r.x + i, r.y, (unsigned char)line[i], style);
    }
}

static VuiNode *pane_node(Pane *pane, float grow, unsigned int basis)
{
    VuiNode *n = node_create();

    n->grow = grow;
    n->basis = basis;
    n->draw = draw_pane;
    n->userdata = pane;

    return n;
}

int main(void)
{
    static Pane header = { "header", VUI_COLOR_CYAN };
    static Pane footer = { "footer", VUI_COLOR_YELLOW };
    static Pane left = { "left(fixed 16)", VUI_COLOR_GREEN };
    static Pane top = { "mid-top", VUI_COLOR_MAGENTA };
    static Pane bottom = { "mid-bottom", VUI_COLOR_BLUE };
    static Pane right = { "right(grow 1)", VUI_COLOR_RED };

    VuiCtx *ui = vui_init();

    if (ui == NULL) {
        fprintf(stderr, "Failed to initialize VaporUI\n");
        return 1;
    }

    VuiNode *root = node_create();
    root->direction = VUI_DIR_COLUMN;

    node_add(root, pane_node(&header, 0, 3));

    VuiNode *body = node_create();
    body->direction = VUI_DIR_ROW;
    body->grow = 1;
    body->gap = 1;
    node_add(root, body);

    node_add(body, pane_node(&left, 0, 16));

    VuiNode *middle = node_create();
    middle->direction = VUI_DIR_COLUMN;
    middle->grow = 2;
    middle->gap = 1;
    node_pad(middle, 1, 1, 1, 1);
    node_add(middle, pane_node(&top, 1, 0));
    node_add(middle, pane_node(&bottom, 1, 0));
    node_add(body, middle);

    node_add(body, pane_node(&right, 1, 0));
    node_add(root, pane_node(&footer, 0, 1));

    for (;;) {
        VuiRect full = { 0, 0, vui_width(ui), vui_height(ui) };

        node_layout(root, full);
        vui_clear(ui);
        node_draw(root, vui_screen(ui));
        vui_flush(ui);

        VuiEvent event;

        do {
            event = vui_poll_event(ui);
        } while (event.type == VUI_EVENT_NONE);

        // plain 'q' arrives as text, Ctrl-C as a modified key
        if ((event.type == VUI_EVENT_TEXT && event.text.codepoint == 'q')
            || (event.type == VUI_EVENT_KEY && event.key.key == VUI_KEY_CHAR
                && event.key.modifiers == VUI_MOD_CTRL && event.key.character == 'c')) {
            break;
        }
    }

    node_destroy(root);
    vui_shutdown(ui);

    return 0;
}
