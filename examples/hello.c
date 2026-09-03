#include <stdint.h>
#include <stdio.h>

#include "../include/vaporui.h"
#include "../src/utf8.h"

typedef struct {
    VuiCtx *ui;
    char status[64];
} App;

static const char *key_name(VuiKey key)
{
    switch (key) {
    case VUI_KEY_CTRL_C:    return "Ctrl-C";
    case VUI_KEY_ENTER:     return "Enter";
    case VUI_KEY_BACKSPACE: return "Backspace";
    case VUI_KEY_TAB:       return "Tab";
    case VUI_KEY_ESCAPE:    return "Escape";
    case VUI_KEY_DELETE:    return "Delete";
    case VUI_KEY_UP:        return "Up";
    case VUI_KEY_DOWN:      return "Down";
    case VUI_KEY_LEFT:      return "Left";
    case VUI_KEY_RIGHT:     return "Right";
    case VUI_KEY_HOME:      return "Home";
    case VUI_KEY_END:       return "End";
    case VUI_KEY_PAGE_UP:   return "Page Up";
    case VUI_KEY_PAGE_DOWN: return "Page Down";
    default:                return "?";
    }
}

static void draw_sidebar(VuiNode *node, VuiScreen *scr, void *userdata)
{
    (void)userdata;
    VuiRect r = node->rect;

    scr_puts(scr, r.x, r.y, "vaporUI hello", (VuiStyle){ .attr = VUI_ATTR_BOLD });
    scr_puts(scr, r.x, r.y + 2, "Ctrl-C to quit", (VuiStyle){0});
}

static void draw_main(VuiNode *node, VuiScreen *scr, void *userdata)
{
    App *app = userdata;
    VuiRect r = node->rect;
    char line[64];

    snprintf(line, sizeof(line), "Terminal resolution: %ux%u", vui_width(app->ui), vui_height(app->ui));
    scr_puts(scr, r.x, r.y, line, (VuiStyle){0});

    if (app->status[0] != '\0') {
        scr_puts(scr, r.x, r.y + 1, app->status, (VuiStyle){ .attr = VUI_ATTR_BOLD });
    }
}

static void layout_and_draw(App *app, VuiNode *root)
{
    VuiRect full = { .x = 0, .y = 0, .width = vui_width(app->ui), .height = vui_height(app->ui) };

    node_layout(root, full);

    vui_clear(app->ui);
    node_draw(root, vui_screen(app->ui));
    vui_flush(app->ui);
}

int main(void)
{
    App app = {0};

    app.ui = vui_init();

    if (app.ui == NULL) {
        fprintf(stderr, "Failed to initialize VaporUI\n");
        return 1;
    }

    VuiNode *root = node_create();
    root->direction = VUI_DIR_ROW;
    root->gap = 2;

    VuiNode *sidebar = node_create();
    sidebar->basis = 20; // fixed width, doesn't grow or shrink
    sidebar->draw = draw_sidebar;
    node_add(root, sidebar);

    VuiNode *main_pane = node_create();
    main_pane->grow = 1; // takes whatever's left
    main_pane->draw = draw_main;
    main_pane->userdata = &app;
    node_add(root, main_pane);

    layout_and_draw(&app, root);

    for (;;) {
        VuiEvent event = vui_poll_event(app.ui);

        switch (event.type) {
        case VUI_EVENT_RESIZE:
            layout_and_draw(&app, root);
            break;

        case VUI_EVENT_KEY:
            if (event.key.key == VUI_KEY_CTRL_C) {
                node_destroy(root);
                vui_shutdown(app.ui);
                return 0;
            }

            snprintf(app.status, sizeof(app.status), "key: %s", key_name(event.key.key));
            layout_and_draw(&app, root);
            break;

        case VUI_EVENT_TEXT: {
            uint8_t utf8[5] = {0};
            int len = vui_utf8_encode(event.text.codepoint, utf8);

            snprintf(app.status, sizeof(app.status), "text: %.*s (U+%04X)", len, utf8, event.text.codepoint);
            layout_and_draw(&app, root);
            break;
        }

        default:
            break;
        }
    }
}
