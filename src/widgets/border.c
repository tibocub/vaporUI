#include "../../include/vaporui.h"

// h, v, then the four corners clockwise from top-left
static const VuiCodepoint glyphs[][6] = {
    [VUI_BORDER_SINGLE]  = { 0x2500, 0x2502, 0x250C, 0x2510, 0x2518, 0x2514 },
    [VUI_BORDER_ROUNDED] = { 0x2500, 0x2502, 0x256D, 0x256E, 0x256F, 0x2570 },
    [VUI_BORDER_DOUBLE]  = { 0x2550, 0x2551, 0x2554, 0x2557, 0x255D, 0x255A },
};

static void border_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiBorder *b = (VuiBorder *)node;
    const VuiCodepoint *g = glyphs[b->border];
    VuiRect r = node->rect;

    (void)userdata;

    if (r.width < 2 || r.height < 2) {
        return;
    }

    unsigned int right = r.x + r.width - 1, bottom = r.y + r.height - 1;

    for (unsigned int x = r.x + 1; x < right; x++) {
        scr_set(scr, x, r.y, g[0], b->style);
        scr_set(scr, x, bottom, g[0], b->style);
    }

    for (unsigned int y = r.y + 1; y < bottom; y++) {
        scr_set(scr, r.x, y, g[1], b->style);
        scr_set(scr, right, y, g[1], b->style);
    }

    scr_set(scr, r.x, r.y, g[2], b->style);
    scr_set(scr, right, r.y, g[3], b->style);
    scr_set(scr, right, bottom, g[4], b->style);
    scr_set(scr, r.x, bottom, g[5], b->style);

    if (b->title != NULL && r.width > 4) {
        // clip so a long title stops before the top-right corner
        VuiRect old = scr_clip(scr, (VuiRect){ r.x + 2, r.y, r.width - 3, 1 });

        scr_set(scr, r.x + 2, r.y, ' ', b->style);
        scr_puts(scr, r.x + 3, r.y, b->title, b->style);
        scr_clip(scr, old);
    }
}

VuiBorder *border_create(const char *title)
{
    VuiBorder *b = (VuiBorder *)node_alloc(sizeof(VuiBorder));

    if (b != NULL) {
        b->title = title;
        b->node.padding = 1;
        b->node.draw = border_draw;
    }

    return b;
}
