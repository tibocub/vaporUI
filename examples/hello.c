#include <stdio.h>

#include "../include/vaporui.h"

unsigned char character;

static const char* non_printable[] = {
    "Null character",
    "Start of Heading",
    "Start of Text",
    "End of Text",
    "End of Transmission",
    "Enquiry",
    "Acknowledge",
    "Bell, Alert",
    "Backspace",
    "Horizontal Tab",
    "Line Feed",
    "Vertical Tabulation",
    "Form Feed",
    "Carriage Return",
    "Shift Out",
    "Shift In",
    "Data Link Escape",
    "Device Control One (XON)",
    "Device Control Two",
    "Device Control Three (XOFF)",
    "Device Control Four",
    "Negative Acknowledge",
    "Synchronous Idle",
    "End of Transmission Block",
    "Cancel",
    "End of medium",
    "Substitute",
    "Escape",
    "File Separator",
    "Group Separator",
    "Record Separator",
    "Unit Separator",
    "Space"
};

void printres(VuiCtx *ctx)
{
    printf(
        "Terminal resolution: %ux%u\n",
        vui_width(ctx),
        vui_height(ctx)
    );
}

int main(void)
{
    VuiCtx *ui;

    ui = vui_init();
	printres(ui);

    if (ui == NULL) {
        fprintf(stderr, "Failed to initialize VaporUI\n");
        return 1;
    }

    for (;;) {
        VuiEvent event = vui_poll_event(ui);

        if (event.type == VUI_EVENT_RESIZE) {
            printres(ui);
            fflush(stdout);
        }

        if (event.type == VUI_EVENT_KEY) {
            if (event.key.key == VUI_KEY_CTRL_C) {
                break;
            }

            if (event.key.key >= 33 && event.key.key <= 126) {
                printf(
                    "key: %u\n",
                    event.key.character
                );

						} else if (event.key.key >= 0 && event.key.key <= 32) {
                printf(
                    "key: %s\n",
                    non_printable[event.key.character]
                );

						} else if (event.key.key == 127) {
                printf("key: Del\n");

            } else {
                printf("Extended ASCII or unvalid input detected");
            }
            fflush(stdout);
        }
    }

    vui_shutdown(ui);

    return 0;
}
