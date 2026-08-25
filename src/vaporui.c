#include <stdlib.h>

#include "../include/vaporui.h"

struct VuiCtx {
    VuiSize size;
};

int vui_platform_get_size(VuiSize *size);
void vui_platform_clear(void);

VuiCtx*vui_init(void)
{
    VuiCtx *ctx;
    ctx = malloc(sizeof(*ctx));

    if (ctx == NULL) {
        return NULL;
    }

    if (vui_platform_get_size(&ctx->size) != 0) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

void vui_shutdown(VuiCtx *ctx)
{
   free(ctx);
}

int vui_get_size(VuiCtx *ctx, VuiSize *size)
{
    // Size not cached yet, simply query the platform
    if (ctx == NULL || size == NULL) {
        return -1;
    }

    return vui_platform_get_size(size);
}

void vui_clear(VuiCtx *ctx)
{
    // ctx not used yet but will be soon useful
    (void)ctx;

    vui_platform_clear();
}
