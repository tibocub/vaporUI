#include "../../include/vaporui.h"

static unsigned int dropdown_measure(VuiNode *node, int row)
{
    VuiDropdown *d = (VuiDropdown *)node;
    unsigned int w = 0;

    for (unsigned int i = 0; row && i < d->count; i++) {
        unsigned int iw = vui_str_width(d->items[i]);

        if (iw > w) w = iw;
    }

    return row ? w + 4 : 1;
}

static void dropdown_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiDropdown *d = (VuiDropdown *)node;
    VuiRect r = node->rect;
    VuiStyle style = node->focused ? vui_theme()->focus : node->hovered ? vui_theme()->hover : vui_theme()->popup;

    (void)userdata;

    scr_fill(scr, r, ' ', style);
    scr_puts(scr, r.x + 1, r.y, d->count > 0 ? d->items[d->selected] : "", style);
    scr_set(scr, r.x + r.width - 2, r.y, 0x25BC, style);
}

static const char *popup_item(VuiList *l, unsigned int i)
{
    return ((VuiDropdown *)l->node.userdata)->items[i];
}

static void popup_select(VuiList *l, unsigned int i)
{
    VuiDropdown *d = l->node.userdata;
    VuiCtx *ui = vui_ctx();

    d->selected = i;
    vui_overlay_remove(ui, l->node.parent); // the border around the list

    if (d->on_change != NULL) {
        d->on_change(&d->node, i);
    }
}

static void open(VuiDropdown *d)
{
    VuiBorder *frame = border_create(NULL);
    VuiList *list = list_create(d->count, popup_item);
    unsigned int rows = d->count < 8 ? d->count : 8;

    if (frame == NULL || list == NULL || d->count == 0) {
        node_destroy(&frame->node);
        node_destroy(&list->node);
        return;
    }

    list->node.userdata = d;
    list->on_select = popup_select;
    list_set_cursor(list, d->selected);
    node_add(&frame->node, &list->node);

    if (vui_overlay_add(vui_ctx(), &frame->node, (VuiRect){ d->node.rect.x, d->node.rect.y + 1, d->node.rect.width, rows + 2 },
                        VUI_OVERLAY_DISMISS) != 0) {
        node_destroy(&frame->node);
    }
}

static int dropdown_event(VuiNode *node, VuiCtx *ui, const VuiEvent *event)
{
    (void)ui;

    if (event->type == VUI_EVENT_KEY && event->key.modifiers == VUI_MOD_NONE
        && (event->key.key == VUI_KEY_ENTER || event->key.key == VUI_KEY_DOWN)) {
        open((VuiDropdown *)node);
        return 1;
    }

    if (event->type == VUI_EVENT_TEXT && event->text.codepoint == ' ') {
        open((VuiDropdown *)node);
        return 1;
    }

    if (event->type == VUI_EVENT_MOUSE && event->mouse.action == VUI_MOUSE_PRESS && event->mouse.button == VUI_MOUSE_LEFT) {
        open((VuiDropdown *)node);
        return 1;
    }

    return 0;
}

VuiDropdown *dropdown_create(const char **items, unsigned int count, void (*on_change)(VuiNode *dropdown, unsigned int index))
{
    VuiDropdown *d = (VuiDropdown *)node_alloc(sizeof(VuiDropdown));

    if (d != NULL) {
        d->items = items;
        d->count = count;
        d->on_change = on_change;
        d->node.basis = VUI_AUTO;
        d->node.measure = dropdown_measure;
        d->node.focusable = 1;
        d->node.draw = dropdown_draw;
        d->node.on_event = dropdown_event;
    }

    return d;
}
