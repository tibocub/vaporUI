#include <stdio.h>

#include "../include/vaporui.h"

int main(void)
{
    VuiCtx *ui;
    VuiSize size;

    ui = vui_init();

    if (ui == NULL) {
        fprintf(stderr, "Failed to initialize VaporUI\n");
        return 1;
    }

    vui_clear(ui);

    if (vui_get_size(ui, &size) != 0) {
        fprintf(stderr, "Failed to get terminal size\n");
        vui_shutdown(ui);
        return 1;
    }

    printf(
        "Terminal resolution: %u columns x %u rows\n",
        size.width,
        size.height
    );

    vui_shutdown(ui);

    return 0;
}
