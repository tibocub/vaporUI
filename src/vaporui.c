#include <stdlib.h>

#include "../include/vaporui.h"

struct VuiCtx {
    VuiSize size;
};

int vui_platform_init(void);
void vui_platform_shutdown(void);

int vui_platform_get_size(VuiSize *size);
void vui_platform_clear(void);
VuiEvent vui_platform_poll_event(void);

VuiCtx *vui_init(void)
{
    VuiCtx *ui;

    if (vui_platform_init() != 0) {
        return NULL;
    }

    ui = malloc(sizeof(*ui));

    if (ui == NULL) {
        vui_platform_shutdown();
        return NULL;
    }

    if (vui_platform_get_size(&ui->size) != 0) {
        free(ui);
        vui_platform_shutdown();
        return NULL;
    }

    vui_platform_clear();

    return ui;
}

void vui_shutdown(VuiCtx *ui)
{
    if (ui == NULL) {
        return;
    }

    vui_platform_shutdown();
    free(ui);
}

unsigned int vui_width(VuiCtx *ui)
{
    return ui->size.width;
}

unsigned int vui_height(VuiCtx *ui)
{
    return ui->size.height;
}

void vui_clear(VuiCtx *ui)
{
    (void)ui;
    vui_platform_clear();
}

VuiEvent vui_poll_event(VuiCtx *ui)
{
    VuiEvent event = vui_platform_poll_event();

    if (event.type == VUI_EVENT_RESIZE) {
        ui->size = event.size;
    }

    return event;
}
