#include <stdio.h>

#include "../include/vaporui.h"

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

static void printres(VuiCtx *ctx)
{
    printf(
        "Terminal resolution: %ux%u\n",
        vui_width(ctx),
        vui_height(ctx)
    );
}

int main(void)
{
    VuiCtx *ui = vui_init();

    if (ui == NULL) {
        fprintf(stderr, "Failed to initialize VaporUI\n");
        return 1;
    }

    printres(ui);

    for (;;) {
        VuiEvent event = vui_poll_event(ui);

        switch (event.type) {
        case VUI_EVENT_RESIZE:
            printres(ui);
            fflush(stdout);
            break;

        case VUI_EVENT_KEY:
            if (event.key.key == VUI_KEY_CTRL_C) {
                vui_shutdown(ui);
                return 0;
            }

            printf("key: %s\n", key_name(event.key.key));
            fflush(stdout);
            break;

        case VUI_EVENT_TEXT:
            printf("text: U+%04X\n", event.text.codepoint);
            fflush(stdout);
            break;

        default:
            break;
        }
    }
}
