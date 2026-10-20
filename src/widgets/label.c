#include "../../include/vaporui.h"

static unsigned int label_measure(VuiNode *node, int row)
{
    const char *s = ((VuiLabel *)node)->text;
    unsigned int width = 0, lines = 1;

    for (const char *line = s, *p = s; ; p++) {
        if (*p == '\n' || *p == '\0') {
            unsigned int w = vui_strn_width(line, (size_t)(p - line));

            if (w > width) width = w;
            if (*p == '\0') break;

            line = p + 1;
            lines++;
        }
    }

    return row ? width : lines;
}

static void label_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiLabel *l = (VuiLabel *)node;
    unsigned int row = 0;

    (void)userdata;

    for (const char *line = l->text, *p = line; ; p++) {
        if (*p == '\n' || *p == '\0') {
            scr_putn(scr, node->rect.x, node->rect.y + row++, line, (size_t)(p - line), l->style);

            if (*p == '\0') break;

            line = p + 1;
        }
    }
}

VuiLabel *label_create(const char *text)
{
    VuiLabel *l = (VuiLabel *)node_alloc(sizeof(VuiLabel));

    if (l != NULL) {
        l->text = text;
        l->style = vui_theme()->text;
        l->node.basis = VUI_AUTO;
        l->node.draw = label_draw;
        l->node.measure = label_measure;
    }

    return l;
}
