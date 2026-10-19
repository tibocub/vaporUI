#include <stdlib.h>

#include "../include/vaporui.h"

struct VuiCtx {
    VuiSize size;
    VuiScreen *screen;
    VuiNode *focus;
};

int vui_platform_init(void);
void vui_platform_shutdown(void);

int vui_platform_get_size(VuiSize *size);
void vui_platform_clear(void);
VuiEvent vui_platform_poll_event(void);
void vui_platform_set_mouse(int enable);

VuiCtx *vui_init(void)
{
    VuiCtx *ui;

    if (vui_platform_init() != 0) {
        return NULL;
    }

    ui = calloc(1, sizeof(*ui));

    if (ui == NULL) {
        vui_platform_shutdown();
        return NULL;
    }

    if (vui_platform_get_size(&ui->size) != 0) {
        free(ui);
        vui_platform_shutdown();
        return NULL;
    }

    ui->screen = scr_create(ui->size.width, ui->size.height);

    if (ui->screen == NULL) {
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

    scr_destroy(ui->screen);
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

// Clears the back buffer -- nothing reaches the terminal until the
// next vui_flush().
void vui_clear(VuiCtx *ui)
{
    scr_clear(ui->screen, (VuiStyle){0});
}

void vui_flush(VuiCtx *ui)
{
    scr_flush(ui->screen);
}

VuiScreen *vui_screen(VuiCtx *ui)
{
    return ui->screen;
}

VuiEvent vui_poll_event(VuiCtx *ui)
{
    VuiEvent event = vui_platform_poll_event();

    if (event.type == VUI_EVENT_RESIZE) {
        ui->size = event.size;
        scr_resize(ui->screen, event.size.width, event.size.height);
    }

    return event;
}

void vui_set_mouse(VuiCtx *ui, int enable)
{
    (void)ui;
    vui_platform_set_mouse(enable);
}

VuiNode *vui_ctx_focus(VuiCtx *ui)
{
    return ui->focus;
}

void vui_ctx_set_focus(VuiCtx *ui, VuiNode *node)
{
    ui->focus = node;
}
