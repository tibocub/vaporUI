#include "../../include/vaporui.h"

static void scroll_by(VuiNode *node, int delta)
{
    unsigned int view = node->rect.height;
    unsigned int max = node->content > view ? node->content - view : 0;
    int pos = (int)node->scroll + delta;

    node->scroll = pos < 0 ? 0 : (unsigned int)pos > max ? max : (unsigned int)pos;
}

static void scroll_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiRect r = node->rect;
    unsigned int thumb, pos, x = r.x + r.width - 1;

    (void)userdata;

    if (node->content <= r.height || r.height == 0 || r.width == 0) {
        return;
    }

    thumb = r.height * r.height / node->content;
    thumb = thumb < 1 ? 1 : thumb;
    pos = node->scroll * (r.height - thumb) / (node->content - r.height);

    for (unsigned int y = 0; y < r.height; y++) {
        int on = y >= pos && y < pos + thumb;

        scr_set(scr, x, r.y + y, on ? 0x2588 : 0x2502, on ? vui_theme()->thumb : vui_theme()->scroll);
    }
}

static int scroll_event(VuiNode *node, VuiCtx *ui, const VuiEvent *event)
{
    unsigned int page = node->rect.height > 1 ? node->rect.height - 1 : 1;

    (void)ui;

    if (event->type == VUI_EVENT_MOUSE) {
        const VuiMouseEvent *m = &event->mouse;

        if (m->button == VUI_MOUSE_WHEEL_UP || m->button == VUI_MOUSE_WHEEL_DOWN) {
            scroll_by(node, m->button == VUI_MOUSE_WHEEL_UP ? -3 : 3);
            return 1;
        }

        // press or drag on the scrollbar column jumps to that position
        if (m->button == VUI_MOUSE_LEFT && (int)m->x == node->rect.x + (int)node->rect.width - 1 && node->rect.height > 1
            && node->content > node->rect.height) {
            int y = (int)m->y - node->rect.y;

            y = y < 0 ? 0 : y;
            node->scroll = 0;
            scroll_by(node, y * (int)(node->content - node->rect.height) / (int)(node->rect.height - 1));
            return 1;
        }

        return 0;
    }

    if (event->type == VUI_EVENT_KEY && event->key.modifiers == VUI_MOD_NONE) {
        switch (event->key.key) {
        case VUI_KEY_UP:        scroll_by(node, -1); return 1;
        case VUI_KEY_DOWN:      scroll_by(node, 1); return 1;
        case VUI_KEY_PAGE_UP:   scroll_by(node, -(int)page); return 1;
        case VUI_KEY_PAGE_DOWN: scroll_by(node, (int)page); return 1;
        case VUI_KEY_HOME:      node->scroll = 0; return 1;
        case VUI_KEY_END:       node->scroll = node->content; scroll_by(node, 0); return 1;
        default:                return 0;
        }
    }

    return 0;
}

VuiNode *scroll_create(void)
{
    VuiNode *n = node_create();

    if (n != NULL) {
        n->direction = VUI_DIR_COLUMN;
        n->scrolls = 1;
        n->grow = 1;
        n->pad_right = 1; // room for the scrollbar
        n->draw = scroll_draw;
        n->on_event = scroll_event;
    }

    return n;
}
