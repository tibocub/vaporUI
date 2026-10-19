#include "../include/vaporui.h"

VuiNode *vui_ctx_focus(VuiCtx *ui);
void vui_ctx_set_focus(VuiCtx *ui, VuiNode *node);

VuiNode *vui_focus(VuiCtx *ui)
{
    return vui_ctx_focus(ui);
}

void vui_set_focus(VuiCtx *ui, VuiNode *node)
{
    VuiNode *old = vui_ctx_focus(ui);

    if (old != NULL) {
        old->focused = 0;
    }

    if (node != NULL) {
        node->focused = 1;
    }

    vui_ctx_set_focus(ui, node);
}

void vui_render(VuiCtx *ui, VuiNode *root)
{
    node_layout(root, (VuiRect){ 0, 0, vui_width(ui), vui_height(ui) });
    vui_clear(ui);
    node_draw(root, vui_screen(ui));
    vui_flush(ui);
}

typedef struct {
    VuiNode *cur, *prev, *next, *first, *last;
    int seen;
} Scan;

static void scan(VuiNode *n, Scan *s)
{
    if (n->focusable) {
        if (s->first == NULL) s->first = n;
        s->last = n;

        if (n == s->cur) s->seen = 1;
        else if (s->seen && s->next == NULL) s->next = n;
        else if (!s->seen) s->prev = n;
    }

    for (unsigned int i = 0; i < n->child_count; i++) {
        scan(n->children[i], s);
    }
}

static void move_focus(VuiCtx *ui, VuiNode *root, int backward)
{
    Scan s = { .cur = vui_focus(ui) };

    scan(root, &s);
    vui_set_focus(ui, backward ? (s.prev ? s.prev : s.last) : (s.next ? s.next : s.first));
}

static int contains(VuiRect r, unsigned int x, unsigned int y)
{
    return x >= r.x && y >= r.y && x - r.x < r.width && y - r.y < r.height;
}

// Deepest node under the point; later siblings are drawn on top.
static VuiNode *node_at(VuiNode *n, unsigned int x, unsigned int y)
{
    if (!contains(n->rect, x, y)) {
        return NULL;
    }

    for (unsigned int i = n->child_count; i-- > 0;) {
        VuiNode *hit = node_at(n->children[i], x, y);

        if (hit != NULL) {
            return hit;
        }
    }

    return n;
}

// Searches down from root rather than following n->parent, since n may
// already be freed.
static int in_tree(VuiNode *root, VuiNode *n)
{
    if (root == n) {
        return 1;
    }

    for (unsigned int i = 0; i < root->child_count; i++) {
        if (in_tree(root->children[i], n)) {
            return 1;
        }
    }

    return 0;
}

int vui_dispatch(VuiCtx *ui, VuiNode *root, const VuiEvent *event)
{
    VuiNode *target;

    // The focused node may have been destroyed or removed since last time.
    if (vui_focus(ui) != NULL && !in_tree(root, vui_focus(ui))) {
        vui_set_focus(ui, NULL);
    }

    switch (event->type) {
    case VUI_EVENT_KEY:
        if (event->key.key == VUI_KEY_TAB && (event->key.modifiers & ~VUI_MOD_SHIFT) == 0) {
            move_focus(ui, root, event->key.modifiers & VUI_MOD_SHIFT);
            return 1;
        }
        target = vui_focus(ui);
        break;

    case VUI_EVENT_TEXT:
    case VUI_EVENT_PASTE_START:
    case VUI_EVENT_PASTE_END:
        target = vui_focus(ui);
        break;

    case VUI_EVENT_MOUSE:
        target = node_at(root, event->mouse.x, event->mouse.y);

        if (event->mouse.action == VUI_MOUSE_PRESS && event->mouse.button == VUI_MOUSE_LEFT) {
            VuiNode *f = target;

            while (f != NULL && !f->focusable) {
                f = f->parent;
            }

            if (f != NULL) {
                vui_set_focus(ui, f);
            }
        }
        break;

    default:
        return 0;
    }

    for (; target != NULL; target = target->parent) {
        if (target->on_event != NULL && target->on_event(target, ui, event)) {
            return 1;
        }
    }

    return 0;
}
