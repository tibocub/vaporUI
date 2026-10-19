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
