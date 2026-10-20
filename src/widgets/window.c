#include "../../include/vaporui.h"

#define MIN_W 12
#define MIN_H 4

// single-line frame: h, v, corners clockwise from top-left
static const VuiCodepoint G[] = { 0x2500, 0x2502, 0x250C, 0x2510, 0x2518, 0x2514 };

static void window_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiWindow *w = (VuiWindow *)node;
    VuiRect r = node->rect;
    VuiStyle frame = vui_theme()->border, bar = node->active ? vui_theme()->title : vui_theme()->title_off;
    unsigned int right = r.x + r.width - 1, bottom = r.y + r.height - 1;

    (void)userdata;

    if (r.width < 4 || r.height < 2) {
        return;
    }

    for (unsigned int x = r.x; x <= right; x++) scr_set(scr, x, bottom, G[0], frame);
    for (unsigned int y = r.y + 1; y < bottom; y++) {
        scr_set(scr, r.x, y, G[1], frame);
        scr_set(scr, right, y, G[1], frame);
    }

    scr_set(scr, r.x, bottom, G[5], frame);
    scr_set(scr, right, bottom, G[4], frame);

    scr_fill(scr, (VuiRect){ r.x, r.y, r.width, 1 }, ' ', bar);

    {
        VuiRect old = scr_clip(scr, (VuiRect){ r.x + 1, r.y, r.width - 5, 1 });

        scr_puts(scr, r.x + 1, r.y, " ", bar);
        scr_puts(scr, r.x + 2, r.y, w->title != NULL ? w->title : "", bar);
        scr_clip(scr, old);
    }

    scr_puts(scr, right - 2, r.y, "[x]", bar);
}

static int window_event(VuiNode *node, VuiCtx *ui, const VuiEvent *event)
{
    VuiWindow *w = (VuiWindow *)node;
    const VuiMouseEvent *m = &event->mouse;
    int right, bottom;

    if (event->type != VUI_EVENT_MOUSE) {
        return 0;
    }

    right = node->rect.x + (int)node->rect.width - 1;
    bottom = node->rect.y + (int)node->rect.height - 1;

    if (m->action == VUI_MOUSE_RELEASE) {
        w->drag = 0;
        return 1;
    }

    if (m->action == VUI_MOUSE_PRESS && m->button == VUI_MOUSE_LEFT) {
        if ((int)m->y == node->rect.y && (int)m->x >= right - 2) {
            if (w->on_close != NULL) w->on_close(node);
            else vui_overlay_remove(ui, node);
            return 1;
        }

        if ((int)m->y == node->rect.y) {
            w->drag = 1;
        } else if ((int)m->y == bottom && (int)m->x == right) {
            w->drag = 2;
        } else {
            return 0;
        }

        w->grab_x = (int)m->x - (w->drag == 1 ? node->rect.x : right);
        w->grab_y = (int)m->y - node->rect.y;
        return 1;
    }

    if (m->action == VUI_MOUSE_MOVE && w->drag == 1) {
        int x = (int)m->x - w->grab_x, y = (int)m->y - w->grab_y;
        int sw = (int)vui_width(ui), sh = (int)vui_height(ui), keep = 4; // the title bar can't leave the screen entirely

        node->rect.x = x < keep - (int)node->rect.width ? keep - (int)node->rect.width : x > sw - keep ? sw - keep : x;
        node->rect.y = y < 0 ? 0 : y > sh - 1 ? sh - 1 : y;
        return 1;
    }

    if (m->action == VUI_MOUSE_MOVE && w->drag == 2) {
        int width = (int)m->x - w->grab_x - node->rect.x + 1, height = (int)m->y - node->rect.y + 1;

        node->rect.width = width < MIN_W ? MIN_W : width;
        node->rect.height = height < MIN_H ? MIN_H : height;
        return 1;
    }

    return 0;
}

VuiWindow *window_create(const char *title)
{
    VuiWindow *w = (VuiWindow *)node_alloc(sizeof(VuiWindow));

    if (w != NULL) {
        w->title = title;
        node_pad(&w->node, 1, 1, 1, 1);
        w->node.draw = window_draw;
        w->node.on_event = window_event;
    }

    return w;
}
