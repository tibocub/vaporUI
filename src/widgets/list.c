#include "../../include/vaporui.h"

static unsigned int list_measure(VuiNode *node, int row)
{
    VuiList *l = (VuiList *)node;
    unsigned int w = 0;

    if (!row) {
        return l->count;
    }

    for (unsigned int i = 0; i < l->count && i < 256; i++) { // shortcut: wide lists beyond 256 rows are measured from the first 256
        unsigned int iw = vui_str_width(l->item(l, i));

        if (iw > w) w = iw;
    }

    return w;
}

static void list_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiList *l = (VuiList *)node;
    VuiRect r = node->rect;

    (void)userdata;

    if (l->follow && r.height > 0) {
        if (l->cursor < l->top) l->top = l->cursor;
        else if (l->cursor >= l->top + r.height) l->top = l->cursor - r.height + 1;
        l->follow = 0;
    }

    for (unsigned int row = 0; row < r.height && l->top + row < l->count; row++) {
        unsigned int i = l->top + row;
        VuiStyle style = vui_theme()->text;

        if (i == l->cursor) {
            if (node->focused) style = vui_theme()->focus;
            else style.attr |= VUI_ATTR_BOLD;
        } else if (node->hovered && i == l->hover_row) {
            style = vui_theme()->hover;
        }

        scr_fill(scr, (VuiRect){ r.x, r.y + row, r.width, 1 }, ' ', style);
        scr_puts(scr, r.x, r.y + row, l->item(l, i), style);
    }
}

static void move_cursor(VuiList *l, unsigned int i)
{
    if (i >= l->count) i = l->count - 1;
    if (i == l->cursor) return;

    l->cursor = i;
    l->follow = 1;

    if (l->on_cursor != NULL) {
        l->on_cursor(l, i);
    }
}

static int list_event(VuiNode *node, VuiCtx *ui, const VuiEvent *event)
{
    VuiList *l = (VuiList *)node;
    unsigned int page = node->rect.height > 1 ? node->rect.height - 1 : 1;

    (void)ui;

    if (event->type == VUI_EVENT_MOUSE) {
        unsigned int row = event->mouse.y >= (unsigned int)node->rect.y ? event->mouse.y - node->rect.y : 0;

        l->hover_row = l->top + row;

        if (event->mouse.button == VUI_MOUSE_WHEEL_UP || event->mouse.button == VUI_MOUSE_WHEEL_DOWN) {
            unsigned int max = l->count > node->rect.height ? l->count - node->rect.height : 0;

            if (event->mouse.button == VUI_MOUSE_WHEEL_UP) l->top = l->top > 3 ? l->top - 3 : 0;
            else l->top = l->top + 3 < max ? l->top + 3 : max;
            return 1;
        }

        if (event->mouse.action == VUI_MOUSE_PRESS && event->mouse.button == VUI_MOUSE_LEFT) {
            if (l->hover_row < l->count) {
                move_cursor(l, l->hover_row);

                if (l->on_select != NULL) {
                    l->on_select(l, l->hover_row);
                }
            }

            return 1;
        }

        return 0;
    }

    if (l->count == 0 || event->type != VUI_EVENT_KEY || event->key.modifiers != VUI_MOD_NONE) {
        return 0;
    }

    switch (event->key.key) {
    case VUI_KEY_UP:        move_cursor(l, l->cursor > 0 ? l->cursor - 1 : 0); return 1;
    case VUI_KEY_DOWN:      move_cursor(l, l->cursor + 1); return 1;
    case VUI_KEY_PAGE_UP:   move_cursor(l, l->cursor > page ? l->cursor - page : 0); return 1;
    case VUI_KEY_PAGE_DOWN: move_cursor(l, l->cursor + page); return 1;
    case VUI_KEY_HOME:      move_cursor(l, 0); return 1;
    case VUI_KEY_END:       move_cursor(l, l->count - 1); return 1;
    case VUI_KEY_ENTER:
        if (l->on_select != NULL) l->on_select(l, l->cursor);
        return 1;
    default:                return 0;
    }
}

void list_set_count(VuiList *l, unsigned int count)
{
    l->count = count;

    if (l->cursor >= count) l->cursor = count > 0 ? count - 1 : 0;
    if (l->top >= count) l->top = count > 0 ? count - 1 : 0;

    l->follow = 1;
}

void list_set_cursor(VuiList *l, unsigned int index)
{
    l->cursor = index < l->count ? index : (l->count > 0 ? l->count - 1 : 0);
    l->follow = 1;
}

VuiList *list_create(unsigned int count, const char *(*item)(VuiList *list, unsigned int index))
{
    VuiList *l = (VuiList *)node_alloc(sizeof(VuiList));

    if (l != NULL) {
        l->item = item;
        l->count = count;
        l->node.grow = 1;
        l->node.basis = VUI_AUTO;
        l->node.measure = list_measure;
        l->node.focusable = 1;
        l->node.draw = list_draw;
        l->node.on_event = list_event;
    }

    return l;
}
