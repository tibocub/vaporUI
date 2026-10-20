#include "ctx.h"

VuiNode *vui_focus(VuiCtx *ui)
{
    return ui->focus;
}

static int in_ui(VuiCtx *ui, VuiNode *root, VuiNode *n);

// Scroll every scrolling ancestor just enough to show node.
static void reveal(VuiNode *node)
{
    for (VuiNode *p = node->parent; p != NULL; p = p->parent) {
        if (!p->scrolls) {
            continue;
        }

        int row = p->direction == VUI_DIR_ROW;
        int start = row ? node->rect.x - p->rect.x : node->rect.y - p->rect.y;
        int size = (int)(row ? node->rect.width : node->rect.height);
        int view = (int)(row ? p->rect.width : p->rect.height);

        if (start < 0) {
            p->scroll = p->scroll > (unsigned int)-start ? p->scroll + start : 0;
        } else if (start + size > view) {
            p->scroll += start + size - view > start ? start : start + size - view; // never push its start out of view
        }
    }
}

void vui_set_focus(VuiCtx *ui, VuiNode *node)
{
    VuiNode *old = ui->focus;

    // old may have been destroyed since it got the focus
    if (old != NULL && (ui->root == NULL || in_ui(ui, ui->root, old))) {
        old->focused = 0;
    }

    if (node != NULL) {
        node->focused = 1;
        reveal(node);
    }

    ui->focus = node;
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

// Index of the topmost modal overlay, or -1.
static int modal_index(VuiCtx *ui)
{
    for (int i = (int)ui->overlay_count - 1; i >= 0; i--) {
        if (ui->overlays[i].flags & VUI_OVERLAY_MODAL) {
            return i;
        }
    }

    return -1;
}

static void move_focus(VuiCtx *ui, VuiNode *root, int backward)
{
    Scan s = { .cur = vui_focus(ui) };
    int modal = modal_index(ui);

    if (modal < 0) {
        scan(root, &s);
    }

    for (unsigned int i = modal < 0 ? 0 : modal; i < ui->overlay_count; i++) {
        scan(ui->overlays[i].node, &s);
    }

    vui_set_focus(ui, backward ? (s.prev ? s.prev : s.last) : (s.next ? s.next : s.first));
}

static VuiNode *first_focusable(VuiNode *node)
{
    Scan s = { 0 };

    scan(node, &s);
    return s.first;
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

static int in_ui(VuiCtx *ui, VuiNode *root, VuiNode *n)
{
    if (in_tree(root, n)) {
        return 1;
    }

    for (unsigned int i = 0; i < ui->overlay_count; i++) {
        if (in_tree(ui->overlays[i].node, n)) {
            return 1;
        }
    }

    return 0;
}

static void set_hover(VuiCtx *ui, VuiNode *root, VuiNode *hit)
{
    if (ui->hover != NULL && in_ui(ui, root, ui->hover)) {
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

void vui_overlays_free(VuiCtx *ui)
{
    while (ui->removed_count > 0) {
        node_destroy(ui->removed[--ui->removed_count]);
    }
}

int vui_overlay_add(VuiCtx *ui, VuiNode *node, VuiRect rect, int flags)
{
    unsigned int sw = vui_width(ui), sh = vui_height(ui);

    if (ui->overlay_count == VUI_MAX_OVERLAYS) {
        return -1;
    }

    if (rect.width == 0) rect.width = node_measure(node, 1);
    if (rect.height == 0) rect.height = node_measure(node, 0);
    if (rect.width > sw) rect.width = sw;
    if (rect.height > sh) rect.height = sh;
    if (rect.x + (int)rect.width > (int)sw) rect.x = sw - rect.width;
    if (rect.y + (int)rect.height > (int)sh) rect.y = sh - rect.height;
    if (rect.x < 0) rect.x = 0;
    if (rect.y < 0) rect.y = 0;

    node->rect = rect;
    node->parent = NULL;
    ui->overlays[ui->overlay_count++] = (VuiOverlay){ node, flags, ui->focus };

    if (first_focusable(node) != NULL) {
        vui_set_focus(ui, first_focusable(node));
    }

    return 0;
}

static int overlay_index(VuiCtx *ui, VuiNode *node)
{
    for (unsigned int i = 0; i < ui->overlay_count; i++) {
        if (ui->overlays[i].node == node) {
            return (int)i;
        }
    }

    return -1;
}

void vui_overlay_remove(VuiCtx *ui, VuiNode *node)
{
    int i;
    VuiNode *back;

    while (node->parent != NULL) {
        node = node->parent;
    }

    i = overlay_index(ui, node);

    if (i < 0) {
        return;
    }

    back = ui->overlays[i].prev_focus;

    for (unsigned int j = i; j + 1 < ui->overlay_count; j++) {
        ui->overlays[j] = ui->overlays[j + 1];
    }

    ui->overlay_count--;

    if (ui->removed_count == VUI_MAX_OVERLAYS) {
        vui_overlays_free(ui);
    }

    ui->removed[ui->removed_count++] = node;

    if (ui->capture != NULL && in_tree(node, ui->capture)) ui->capture = NULL;
    if (ui->hover != NULL && in_tree(node, ui->hover)) ui->hover = NULL;

    if (ui->focus != NULL && in_tree(node, ui->focus)) {
        // back may have been freed since the overlay opened
        int valid = back != NULL && ui->root != NULL && !in_tree(node, back) && in_ui(ui, ui->root, back);

        vui_set_focus(ui, valid ? back : NULL);
    }
}

void vui_overlay_raise(VuiCtx *ui, VuiNode *node)
{
    int i = overlay_index(ui, node);
    VuiOverlay o;

    if (i < 0 || (unsigned int)i + 1 == ui->overlay_count) {
        return;
    }

    o = ui->overlays[i];

    for (unsigned int j = i; j + 1 < ui->overlay_count; j++) {
        ui->overlays[j] = ui->overlays[j + 1];
    }

    ui->overlays[ui->overlay_count - 1] = o;
}

void vui_render(VuiCtx *ui, VuiNode *root)
{
    ui->root = root;
    vui_overlays_free(ui);
    node_layout(root, (VuiRect){ 0, 0, vui_width(ui), vui_height(ui) });
    scr_clear(vui_screen(ui), vui_theme()->text);
    node_draw(root, vui_screen(ui));

    for (unsigned int i = 0; i < ui->overlay_count; i++) {
        VuiNode *n = ui->overlays[i].node;

        n->active = i + 1 == ui->overlay_count;
        node_layout(n, n->rect);
        scr_fill(vui_screen(ui), n->rect, ' ', vui_theme()->text);
        node_draw(n, vui_screen(ui));
    }

    vui_flush(ui);
}

// Topmost node under the point, looking at overlays first; *overlay gets its index or -1.
static VuiNode *hit_test(VuiCtx *ui, VuiNode *root, unsigned int x, unsigned int y, int *overlay)
{
    int modal = modal_index(ui);

    for (int i = (int)ui->overlay_count - 1; i >= (modal < 0 ? 0 : modal); i--) {
        VuiNode *hit = node_at(ui->overlays[i].node, x, y);

        if (hit != NULL) {
            *overlay = i;
            return hit;
        }
    }

    *overlay = -1;
    return modal < 0 ? node_at(root, x, y) : NULL;
}

int vui_dispatch(VuiCtx *ui, VuiNode *root, const VuiEvent *event)
{
    VuiNode *target, *handler;
    int modal, hit_overlay = -1;

    ui->root = root;
    vui_overlays_free(ui);

    // The focused node may have been destroyed or removed since last time.
    if (ui->focus != NULL && !in_ui(ui, root, ui->focus)) {
        ui->focus = NULL; // not via vui_set_focus(): the old node may be freed
    }

    if (ui->capture != NULL && !in_ui(ui, root, ui->capture)) {
        ui->capture = NULL;
    }

    // A modal overlay keeps the focus to itself
    modal = modal_index(ui);

    if (modal >= 0) {
        int inside = 0;

        for (unsigned int i = modal; i < ui->overlay_count; i++) {
            inside |= ui->focus != NULL && in_tree(ui->overlays[i].node, ui->focus);
        }

        if (!inside) {
            vui_set_focus(ui, first_focusable(ui->overlays[modal].node));
        }
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
        target = hit_test(ui, root, event->mouse.x, event->mouse.y, &hit_overlay);
        set_hover(ui, root, target);

        if (ui->capture != NULL) {
            VuiNode *owner = ui->capture;

            if (event->mouse.action == VUI_MOUSE_RELEASE || event->mouse.button == VUI_MOUSE_NONE) {
                ui->capture = NULL;
            }

            return bubble(owner, ui, event) != NULL;
        }

        if (event->mouse.action == VUI_MOUSE_PRESS && ui->overlay_count > 0) {
            VuiOverlay top = ui->overlays[ui->overlay_count - 1];

            if ((top.flags & VUI_OVERLAY_DISMISS) && hit_overlay != (int)ui->overlay_count - 1) {
                vui_overlay_remove(ui, top.node);
                return 1;
            }

            if (hit_overlay >= 0 && event->mouse.button == VUI_MOUSE_LEFT) {
                VuiNode *window = ui->overlays[hit_overlay].node;

                vui_overlay_raise(ui, window);

                if (ui->focus == NULL || !in_tree(window, ui->focus)) {
                    vui_set_focus(ui, first_focusable(window));
                }
            }
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

    if (handler == NULL && event->type == VUI_EVENT_KEY && event->key.key == VUI_KEY_ESCAPE
        && event->key.modifiers == VUI_MOD_NONE && ui->overlay_count > 0
        && (ui->overlays[ui->overlay_count - 1].flags & VUI_OVERLAY_DISMISS)) {
        vui_overlay_remove(ui, ui->overlays[ui->overlay_count - 1].node);
        return 1;
    }

    return handler != NULL;
}

void vui_quit(VuiCtx *ui)
{
    ui->quit = 1;
}

void vui_run(VuiCtx *ui, VuiNode *root, VuiAppHandler handler, void *userdata)
{
    ui->quit = 0;

    while (!ui->quit) {
        VuiEvent event;

        vui_render(ui, root);
        event = vui_poll_event(ui);

        if (event.type == VUI_EVENT_CLOSE) {
            break;
        }

        if (event.type == VUI_EVENT_NONE || (handler != NULL && handler(ui, &event, userdata))) {
            continue;
        }

        vui_dispatch(ui, root, &event);
    }
}
