#include "../../include/vaporui.h"

static void div_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    (void)userdata;
    scr_fill(scr, node->rect, ' ', ((VuiDiv *)node)->style);
}

VuiDiv *div_create(VuiDirection direction)
{
    VuiDiv *div = (VuiDiv *)node_alloc(sizeof(VuiDiv));

    if (div != NULL) {
        div->node.direction = direction;
        div->node.draw = div_draw;
    }

    return div;
}

VuiDiv *bar_create(void)
{
    VuiDiv *bar = div_create(VUI_DIR_ROW);

    if (bar != NULL) {
        bar->style = vui_theme()->bar;
        bar->node.basis = 1;
        bar->node.shrink = 0;
        bar->node.gap = 1;
        node_pad(&bar->node, 0, 1, 0, 1);
    }

    return bar;
}
