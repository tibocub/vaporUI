// colors: 16 / 256 / truecolor, plus justify and align.
//
// Verify:
//   - row 1: the 16 ANSI colors as background swatches, the bright ones on the right
//   - row 2: the 256-color palette (6x6x6 cube and the gray ramp)
//   - row 3: a smooth truecolor gradient (banded on a 256-color terminal, 8 steps on a
//     16-color one: try `COLORTERM= TERM=xterm build/colors` and `TERM=linux build/colors`)
//   - below: boxes showing justify (start / center / end / between) and cross-axis align
//     (stretch / start / center / end) with the same two buttons; resize to see them follow
//   - Ctrl-C quits

#include <stdint.h>

#include "../include/vaporui.h"

static void draw_swatches(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiRect r = node->rect;
    int mode = (int)(intptr_t)userdata;

    for (unsigned int x = 0; x < r.width; x++) {
        VuiColor c;

        if (mode == 0) {
            c = 1 + x / 2 % 16;
        } else if (mode == 1) {
            c = VUI_COLOR256(16 + x * 216 / r.width);
        } else {
            unsigned int v = x * 255 / (r.width > 1 ? r.width - 1 : 1);

            c = VUI_RGB(v, 255 - v, 160 + v / 4);
        }

        scr_set(scr, r.x + x, r.y, ' ', (VuiStyle){ .bg = c });
    }
}

static VuiNode *swatch(VuiNode *parent, int mode)
{
    VuiNode *n = node_create();

    n->basis = 1;
    n->draw = draw_swatches;
    n->userdata = (void *)(intptr_t)mode;
    node_add(parent, n);

    return n;
}

// A framed row of two buttons showing one justify / align setting.
static void demo(VuiNode *parent, const char *title, VuiJustify justify, VuiAlign align)
{
    VuiBorder *box = border_create(title);
    VuiNode *inner = node_create();

    box->node.grow = 1;
    box->node.basis = 0;
    inner->grow = 1;
    inner->justify = justify;
    inner->align = align;
    inner->gap = 1;
    node_add(inner, &button_create("A", NULL)->node);
    node_add(inner, &button_create("B", NULL)->node);
    node_add(&box->node, inner);
    node_add(parent, &box->node);
}

static int handler(VuiCtx *ui, const VuiEvent *e, void *userdata)
{
    (void)userdata;

    if (e->type == VUI_EVENT_KEY && e->key.key == VUI_KEY_CHAR && e->key.character == 'c' && e->key.modifiers == VUI_MOD_CTRL) {
        vui_quit(ui);
        return 1;
    }

    return 0;
}

int main(void)
{
    VuiCtx *ui = vui_init();
    VuiNode *root, *justify, *align;

    if (ui == NULL) {
        return 1;
    }

    root = node_create();
    root->direction = VUI_DIR_COLUMN;
    node_pad(root, 0, 1, 0, 1);

    swatch(root, 0);
    swatch(root, 1);
    swatch(root, 2);

    justify = node_create();
    justify->grow = 1;
    justify->gap = 1;
    demo(justify, "start", VUI_JUSTIFY_START, VUI_ALIGN_STRETCH);
    demo(justify, "center", VUI_JUSTIFY_CENTER, VUI_ALIGN_STRETCH);
    demo(justify, "end", VUI_JUSTIFY_END, VUI_ALIGN_STRETCH);
    demo(justify, "between", VUI_JUSTIFY_BETWEEN, VUI_ALIGN_STRETCH);
    node_add(root, justify);

    align = node_create();
    align->grow = 1;
    align->gap = 1;
    demo(align, "stretch", VUI_JUSTIFY_START, VUI_ALIGN_STRETCH);
    demo(align, "align start", VUI_JUSTIFY_START, VUI_ALIGN_START);
    demo(align, "align center", VUI_JUSTIFY_START, VUI_ALIGN_CENTER);
    demo(align, "align end", VUI_JUSTIFY_START, VUI_ALIGN_END);
    node_add(root, align);

    vui_run(ui, root, handler, NULL);

    node_destroy(root);
    vui_shutdown(ui);

    return 0;
}
