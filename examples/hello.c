#include <stdio.h>

#include "../include/vaporui.h"

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

            printf(
                "key: %u\n",
                event.key.character
            );
            fflush(stdout);
        }
    }

    vui_shutdown(ui);

    return 0;
}
