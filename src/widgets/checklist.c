#include <string.h>

#include "../../include/vaporui.h"

static void checklist_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiChecklist *l = (VuiChecklist *)node;
    VuiRect r = node->rect;

    (void)userdata;

    for (unsigned int row = 0; row < r.height && l->top + row < l->count; row++) {
        unsigned int i = l->top + row;
        VuiStyle style = { 0 };

        if (i == l->cursor) {
            style.attr = node->focused ? VUI_ATTR_REVERSE : VUI_ATTR_BOLD;
        } else if (node->hovered && i == l->hover_row) {
            style = (VuiStyle){ .fg = VUI_COLOR_WHITE, .bg = VUI_COLOR_BLUE };
        }

        scr_fill(scr, (VuiRect){ r.x, r.y + row, r.width, 1 }, ' ', style);
        scr_puts(scr, r.x, r.y + row, l->radio ? (l->checked[i] ? "(o) " : "( ) ") : (l->checked[i] ? "[x] " : "[ ] "), style);
        scr_puts(scr, r.x + 4, r.y + row, l->items[i], style);
    }
}

static void toggle(VuiChecklist *l, unsigned int i)
{
    if (l->radio) {
        memset(l->checked, 0, l->count);
        l->checked[i] = 1;
    } else {
        l->checked[i] = !l->checked[i];
    }

    if (l->on_change != NULL) {
        l->on_change(&l->node, i);
    }
}

static void move_cursor(VuiChecklist *l, unsigned int i)
{
    unsigned int h = l->node.rect.height;

    l->cursor = i;

    if (i < l->top) {
        l->top = i;
    } else if (h > 0 && i >= l->top + h) {
        l->top = i - h + 1;
    }
}

static int checklist_event(VuiNode *node, VuiCtx *ui, const VuiEvent *event)
{
    VuiChecklist *l = (VuiChecklist *)node;

    (void)ui;

    if (l->count == 0) {
        return 0;
    }

    if (event->type == VUI_EVENT_MOUSE) {
        l->hover_row = l->top + (event->mouse.y - node->rect.y);
    }

    if (event->type == VUI_EVENT_KEY && event->key.modifiers == VUI_MOD_NONE) {
        switch (event->key.key) {
        case VUI_KEY_UP:    move_cursor(l, l->cursor > 0 ? l->cursor - 1 : 0); return 1;
        case VUI_KEY_DOWN:  move_cursor(l, l->cursor + 1 < l->count ? l->cursor + 1 : l->cursor); return 1;
        case VUI_KEY_HOME:  move_cursor(l, 0); return 1;
        case VUI_KEY_END:   move_cursor(l, l->count - 1); return 1;
        case VUI_KEY_ENTER: toggle(l, l->cursor); return 1;
        default:            return 0;
        }
    }

    if (event->type == VUI_EVENT_TEXT && event->text.codepoint == ' ') {
        toggle(l, l->cursor);
        return 1;
    }

    if (event->type == VUI_EVENT_MOUSE && event->mouse.action == VUI_MOUSE_PRESS
        && event->mouse.button == VUI_MOUSE_LEFT) {
        unsigned int i = l->top + (event->mouse.y - node->rect.y);

        if (i < l->count) {
            move_cursor(l, i);
            toggle(l, i);
        }

        return 1;
    }

    return 0;
}

VuiChecklist *checklist_create(const char **items, unsigned int count, int radio)
{
    VuiChecklist *l = (VuiChecklist *)node_alloc(sizeof(VuiChecklist) + count);

    if (l != NULL) {
        l->items = items;
        l->count = count;
        l->radio = radio;

        if (radio && count > 0) {
            l->checked[0] = 1;
        }

        l->node.grow = 1;
        l->node.focusable = 1;
        l->node.draw = checklist_draw;
        l->node.on_event = checklist_event;
    }

    return l;
}
