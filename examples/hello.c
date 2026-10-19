#include <stdint.h>
#include <stdio.h>

#include "../include/vaporui.h"
#include "../src/utf8.h"

typedef struct {
    VuiCtx *ui;
    char status[64];
    int running;
} App;

static const char *base_name(const VuiKeyEvent *event)
{
    switch (event->key) {
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
    case VUI_KEY_INSERT:    return "Insert";
    default:
        if (event->key >= VUI_KEY_F1 && event->key <= VUI_KEY_F12) {
            static char f[4];
            snprintf(f, sizeof f, "F%d", event->key - VUI_KEY_F1 + 1);
            return f;
        }
        return "?";
    }
}

static const char *key_name(const VuiKeyEvent *event, char *buf, size_t bufsize)
{
    VuiModifiers m = event->modifiers;
    int n = snprintf(buf, bufsize, "%s%s%s%s",
        (m & VUI_MOD_CTRL) ? "Ctrl-" : "",
        (m & VUI_MOD_ALT) ? "Alt-" : "",
        (m & VUI_MOD_SHIFT) ? "Shift-" : "",
        event->key == VUI_KEY_CHAR ? "" : base_name(event));

    if (event->key == VUI_KEY_CHAR && n > 0 && (size_t)n + 1 < bufsize) {
        buf[n] = (char)event->character;
        buf[n + 1] = 0;
    }

    return buf;
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

static void act_quit(VuiCtx *ui, const VuiKeyEvent *event, void *userdata)
{
    (void)ui;
    (void)event;

    ((App *)userdata)->running = 0;
}

int main(void)
{
    App app = { .running = 1 };

    app.ui = vui_init();

    if (app.ui == NULL) {
        fprintf(stderr, "Failed to initialize VaporUI\n");
        return 1;
    }

    VuiControls *global = ctl_create();
    ctl_bind(global, VUI_KEY_CHAR, VUI_MOD_CTRL, 'c', act_quit, &app);

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

    while (app.running) {
        VuiEvent event = vui_poll_event(app.ui);

        if (event.type == VUI_EVENT_KEY && ctl_dispatch(global, app.ui, &event.key)) {
            continue;
        }

        switch (event.type) {
        case VUI_EVENT_RESIZE:
            layout_and_draw(&app, root);
            break;

        case VUI_EVENT_KEY: {
            char name_buf[32];

            snprintf(app.status, sizeof(app.status), "key: %s", key_name(&event.key, name_buf, sizeof(name_buf)));
            layout_and_draw(&app, root);
            break;
        }

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

    node_destroy(root);
    ctl_destroy(global);
    vui_shutdown(app.ui);

    return 0;
}
