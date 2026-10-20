#include "ctx.h"

VuiNode *vui_focus(VuiCtx *ui)
{
    return ui->focus;
}

void vui_set_focus(VuiCtx *ui, VuiNode *node)
{
    VuiNode *old = ui->focus;

    if (old != NULL) {
        old->focused = 0;
    }

    if (node != NULL) {
        node->focused = 1;
    }

    ui->focus = node;
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
    return (int)x >= r.x && (int)y >= r.y && (int)x - r.x < (int)r.width && (int)y - r.y < (int)r.height;
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

static void set_hover(VuiCtx *ui, VuiNode *root, VuiNode *hit)
{
    if (ui->hover != NULL && in_tree(root, ui->hover)) {
        for (VuiNode *n = ui->hover; n != NULL; n = n->parent) {
            n->hovered = 0;
        }
    }

    ui->hover = hit;

    for (VuiNode *n = hit; n != NULL; n = n->parent) {
        n->hovered = 1;
    }
}

// Returns the node that handled the event, or NULL.
static VuiNode *bubble(VuiNode *target, VuiCtx *ui, const VuiEvent *event)
{
    for (; target != NULL; target = target->parent) {
        if (target->on_event != NULL && target->on_event(target, ui, event)) {
            return target;
        }
    }

    return NULL;
}

int vui_dispatch(VuiCtx *ui, VuiNode *root, const VuiEvent *event)
{
    VuiNode *target, *handler;

    // The focused node may have been destroyed or removed since last time.
    if (ui->focus != NULL && !in_tree(root, ui->focus)) {
        ui->focus = NULL; // not via vui_set_focus(): the old node may be freed
    }

    if (ui->capture != NULL && !in_tree(root, ui->capture)) {
        ui->capture = NULL;
    }

    switch (event->type) {
    case VUI_EVENT_KEY:
        // Ctrl-Tab always moves focus, even out of a node that wants Tab
        if (event->key.key == VUI_KEY_TAB && (event->key.modifiers & ~(VUI_MOD_SHIFT | VUI_MOD_CTRL)) == 0) {
            if (!(event->key.modifiers & VUI_MOD_CTRL) && vui_focus(ui) != NULL
                && vui_focus(ui)->wants_tab && bubble(vui_focus(ui), ui, event) != NULL) {
                return 1;
            }

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
        set_hover(ui, root, target);

        if (ui->capture != NULL) {
            VuiNode *owner = ui->capture;

            if (event->mouse.action == VUI_MOUSE_RELEASE || event->mouse.button == VUI_MOUSE_NONE) {
                ui->capture = NULL;
            }

            return bubble(owner, ui, event) != NULL;
        }

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

    handler = bubble(target, ui, event);

    if (handler != NULL && event->type == VUI_EVENT_MOUSE && event->mouse.action == VUI_MOUSE_PRESS
        && event->mouse.button == VUI_MOUSE_LEFT) {
        ui->capture = handler;
    }

    return handler != NULL;
}
