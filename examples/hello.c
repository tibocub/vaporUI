#include <stdint.h>
#include <stdio.h>

#include "../include/vaporui.h"
#include "../src/utf8.h"

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

static void draw(VuiCtx *ui, const char *status)
{
    VuiScreen *scr = vui_screen(ui);
    char line[64];

    vui_clear(ui);

    snprintf(line, sizeof(line), "Terminal resolution: %ux%u", vui_width(ui), vui_height(ui));
    scr_puts(scr, 0, 0, line, (VuiStyle){0});

    if (status != NULL) {
        scr_puts(scr, 0, 1, status, (VuiStyle){ .attr = VUI_ATTR_BOLD });
    }

    scr_puts(scr, 0, 3, "Ctrl-C to quit", (VuiStyle){0});

    vui_flush(ui);
}

int main(void)
{
    VuiCtx *ui = vui_init();

    if (ui == NULL) {
        fprintf(stderr, "Failed to initialize VaporUI\n");
        return 1;
    }

    draw(ui, NULL);

    for (;;) {
        VuiEvent event = vui_poll_event(ui);

        switch (event.type) {
        case VUI_EVENT_RESIZE:
            draw(ui, NULL);
            break;

        case VUI_EVENT_KEY:
            if (event.key.key == VUI_KEY_CTRL_C) {
                vui_shutdown(ui);
                return 0;
            }

            {
                char status[64];
                snprintf(status, sizeof(status), "key: %s", key_name(event.key.key));
                draw(ui, status);
            }
            break;

        case VUI_EVENT_TEXT: {
            uint8_t utf8[5] = {0};
            int len = vui_utf8_encode(event.text.codepoint, utf8);
            char status[64];

            snprintf(status, sizeof(status), "text: %.*s (U+%04X)", len, utf8, event.text.codepoint);
            draw(ui, status);
            break;
        }

        default:
            break;
        }
    }
}
