#include "../../include/vaporui.h"

static unsigned int label_measure(VuiNode *node, int row)
{
    const char *s = ((VuiLabel *)node)->text;
    unsigned int width = 0, line = 0, lines = 1;

    for (const char *p = s; ; p++) {
        if (*p == '\n' || *p == '\0') {
            char buf[256];
            unsigned int n = (unsigned int)(p - (s + line));

            if (n >= sizeof(buf)) n = sizeof(buf) - 1;
            for (unsigned int i = 0; i < n; i++) buf[i] = s[line + i];
            buf[n] = '\0';

            if (vui_str_width(buf) > width) width = vui_str_width(buf);
            if (*p == '\0') break;

            line = (unsigned int)(p - s) + 1;
            lines++;
        }
    }

    return row ? width : lines;
}

static void label_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiLabel *l = (VuiLabel *)node;
    char buf[256];
    unsigned int row = 0, n = 0;

    (void)userdata;

    for (const char *p = l->text; ; p++) {
        if (*p == '\n' || *p == '\0') {
            buf[n] = '\0';
            scr_puts(scr, node->rect.x, node->rect.y + row++, buf, l->style);
            n = 0;

            if (*p == '\0') break;
        } else if (n < sizeof(buf) - 1) {
            buf[n++] = *p;
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
