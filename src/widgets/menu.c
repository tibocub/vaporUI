#include <stdlib.h>

#include "../../include/vaporui.h"

typedef struct {
    const char **items;
    unsigned int count;
    void (*pick)(void *ud, unsigned int index);
    void *ud;
    VuiMenubar *bar; // set for a menu bar's drop-down: Left/Right switch menus
} Popup;

static const char *popup_item(VuiList *l, unsigned int i)
{
    return ((Popup *)l->node.userdata)->items[i];
}

static void free_popup(VuiNode *node)
{
    free(node->userdata);
}

static void popup_select(VuiList *l, unsigned int i)
{
    Popup p = *(Popup *)l->node.userdata; // the overlay is freed after the callback returns

    vui_overlay_remove(vui_ctx(), &l->node);
    p.pick(p.ud, i);
}

static void bar_open(VuiMenubar *b, unsigned int menu);

static int popup_event(VuiNode *node, VuiCtx *ui, const VuiEvent *event)
{
    Popup *p = node->userdata;
    int step = event->type == VUI_EVENT_KEY && event->key.modifiers == VUI_MOD_NONE
        ? (event->key.key == VUI_KEY_RIGHT) - (event->key.key == VUI_KEY_LEFT) : 0;

    if (p->bar == NULL || step == 0) {
        return 0;
    }

    vui_overlay_remove(ui, node);
    bar_open(p->bar, (p->bar->active + p->bar->count + step) % p->bar->count);
    return 1;
}

static void popup_open(int x, int y, const char **items, unsigned int count,
                       void (*pick)(void *ud, unsigned int index), void *ud, VuiMenubar *bar)
{
    VuiBorder *frame = border_create(NULL);
    VuiList *list = list_create(count, popup_item);
    Popup *p = malloc(sizeof(Popup));
    unsigned int w = 0;

    if (frame == NULL || list == NULL || p == NULL || count == 0) {
        node_destroy(&frame->node);
        node_destroy(&list->node);
        free(p);
        return;
    }

    for (unsigned int i = 0; i < count; i++) {
        unsigned int iw = vui_str_width(items[i]);

        if (iw > w) w = iw;
    }

    *p = (Popup){ items, count, pick, ud, bar };
    frame->node.userdata = list->node.userdata = p;
    frame->node.on_event = popup_event;
    list->node.destroy = free_popup; // frame and list share p; the list owns it
    list->on_select = popup_select;
    node_add(&frame->node, &list->node);

    if (vui_overlay_add(vui_ctx(), &frame->node, (VuiRect){ x, y, w + 2, (count < 12 ? count : 12) + 2 },
                        VUI_OVERLAY_DISMISS) != 0) {
        node_destroy(&frame->node);
    }
}

void menu_popup(int x, int y, const char **items, unsigned int count, void (*on_pick)(void *ud, unsigned int index), void *ud)
{
    popup_open(x, y, items, count, on_pick, ud, NULL);
}

static void bar_pick(void *ud, unsigned int index)
{
    VuiMenubar *b = ud;

    if (b->on_pick != NULL) {
        b->on_pick(b, b->active, index);
    }
}

static unsigned int title_x(VuiMenubar *b, unsigned int menu)
{
    unsigned int x = 0;

    for (unsigned int i = 0; i < menu; i++) {
        x += vui_str_width(b->menus[i].title) + 2;
    }

    return x;
}

static void bar_open(VuiMenubar *b, unsigned int menu)
{
    b->active = menu;
    popup_open(b->node.rect.x + title_x(b, menu), b->node.rect.y + 1, b->menus[menu].items, b->menus[menu].count, bar_pick, b, b);
}

static unsigned int bar_measure(VuiNode *node, int row)
{
    VuiMenubar *b = (VuiMenubar *)node;

    return row ? title_x(b, b->count) : 1;
}

static void bar_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiMenubar *b = (VuiMenubar *)node;
    VuiRect r = node->rect;

    (void)userdata;

    for (unsigned int i = 0; i < b->count; i++) {
        VuiStyle style = node->focused && i == b->active ? vui_theme()->focus : vui_theme()->bar;
        int x = r.x + title_x(b, i);

        scr_puts(scr, x, r.y, " ", style);
        scr_puts(scr, x + 1, r.y, b->menus[i].title, style);
        scr_puts(scr, x + 1 + vui_str_width(b->menus[i].title), r.y, " ", style);
    }
}

static int bar_event(VuiNode *node, VuiCtx *ui, const VuiEvent *event)
{
    VuiMenubar *b = (VuiMenubar *)node;

    (void)ui;

    if (event->type == VUI_EVENT_MOUSE && event->mouse.action == VUI_MOUSE_PRESS && event->mouse.button == VUI_MOUSE_LEFT) {
        unsigned int x = event->mouse.x >= (unsigned int)node->rect.x ? event->mouse.x - node->rect.x : 0;

        for (unsigned int i = 0; i < b->count; i++) {
            if (x < title_x(b, i + 1)) {
                bar_open(b, i);
                return 1;
            }
        }

        return 0;
    }

    if (event->type == VUI_EVENT_KEY && event->key.modifiers == VUI_MOD_NONE) {
        switch (event->key.key) {
        case VUI_KEY_LEFT:  b->active = (b->active + b->count - 1) % b->count; return 1;
        case VUI_KEY_RIGHT: b->active = (b->active + 1) % b->count; return 1;
        case VUI_KEY_ENTER:
        case VUI_KEY_DOWN:  bar_open(b, b->active); return 1;
        default: break;
        }
    }

    return 0;
}

VuiMenubar *menubar_create(const VuiMenu *menus, unsigned int count, void (*on_pick)(VuiMenubar *bar, unsigned int menu, unsigned int item))
{
    VuiMenubar *b = count > 0 ? (VuiMenubar *)node_alloc(sizeof(VuiMenubar)) : NULL;

    if (b != NULL) {
        b->menus = menus;
        b->count = count;
        b->on_pick = on_pick;
        b->node.basis = VUI_AUTO;
        b->node.measure = bar_measure;
        b->node.focusable = 1;
        b->node.draw = bar_draw;
        b->node.on_event = bar_event;
    }

    return b;
}
