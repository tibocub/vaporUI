
#include "../../include/vaporui.h"

static unsigned int button_measure(VuiNode *node, int row)
{
    return row ? vui_str_width(((VuiButton *)node)->label) + 4 : 1;
}

static void button_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiButton *b = (VuiButton *)node;
    VuiRect r = node->rect;
    VuiStyle style = { .fg = VUI_COLOR_WHITE, .bg = VUI_COLOR_BLACK };
    unsigned int len = vui_str_width(b->label) + 4; // "[ " + label + " ]"
    unsigned int x = r.x + (r.width > len ? (r.width - len) / 2 : 0);

    (void)userdata;

    // the whole rect is clickable, so the whole rect shows the state
    if (node->focused) {
        style = (VuiStyle){ .attr = VUI_ATTR_REVERSE | VUI_ATTR_BOLD };
    } else if (node->hovered) {
        style = (VuiStyle){ .fg = VUI_COLOR_WHITE, .bg = VUI_COLOR_BLUE, .attr = VUI_ATTR_BOLD };
    }

    scr_fill(scr, r, ' ', style);
    scr_puts(scr, x, r.y + r.height / 2, "[ ", style);
    scr_puts(scr, x + 2, r.y + r.height / 2, b->label, style);
    scr_puts(scr, x + len - 2, r.y + r.height / 2, " ]", style);
}

static int button_event(VuiNode *node, VuiCtx *ui, const VuiEvent *event)
{
    VuiButton *b = (VuiButton *)node;
    int activate = 0;

    (void)ui;

    if (event->type == VUI_EVENT_KEY) {
        activate = event->key.modifiers == VUI_MOD_NONE && event->key.key == VUI_KEY_ENTER;
    } else if (event->type == VUI_EVENT_TEXT) {
        activate = event->text.codepoint == ' ';
    } else if (event->type == VUI_EVENT_MOUSE) {
        activate = event->mouse.action == VUI_MOUSE_PRESS && event->mouse.button == VUI_MOUSE_LEFT;
    }

    if (activate && b->on_click != NULL) {
        b->on_click(node);
    }

    return activate;
}

VuiButton *button_create(const char *label, void (*on_click)(VuiNode *button))
{
    VuiButton *b = (VuiButton *)node_alloc(sizeof(VuiButton));

    if (b != NULL) {
        b->label = label;
        b->on_click = on_click;
        b->node.basis = VUI_AUTO;
        b->node.measure = button_measure;
        b->node.focusable = 1;
        b->node.draw = button_draw;
        b->node.on_event = button_event;
    }

    return b;
}
