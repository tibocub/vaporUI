#include <stdlib.h>

#include "../include/vaporui.h"

VuiNode *node_alloc(size_t size)
{
    VuiNode *node = calloc(1, size);

    if (node == NULL) {
        return NULL;
    }

    node->shrink = 1; // CSS flex-shrink defaults to 1; grow/basis/gap/padding default to 0, justify START, align STRETCH

    return node;
}

VuiNode *node_create(void)
{
    return node_alloc(sizeof(VuiNode));
}

void node_destroy(VuiNode *node)
{
    if (node == NULL) {
        return;
    }

    for (unsigned int i = 0; i < node->child_count; i++) {
        node_destroy(node->children[i]);
    }

    if (node->destroy != NULL) {
        node->destroy(node);
    }

    free(node->children);
    free(node);
}

void node_add(VuiNode *parent, VuiNode *child)
{
    if (parent->child_count == parent->child_cap) {
        unsigned int cap = parent->child_cap ? parent->child_cap * 2 : 4;
        VuiNode **children = realloc(parent->children, cap * sizeof(VuiNode *));

        if (children == NULL) {
            return;
        }

        parent->children = children;
        parent->child_cap = cap;
    }

    parent->children[parent->child_count++] = child;
    child->parent = parent;
}

void node_remove(VuiNode *child)
{
    VuiNode *parent = child->parent;

    if (parent == NULL) {
        return;
    }

    for (unsigned int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] != child) {
            continue;
        }

        for (unsigned int j = i; j + 1 < parent->child_count; j++) {
            parent->children[j] = parent->children[j + 1];
        }

        parent->child_count--;
        child->parent = NULL;
        return;
    }
}

void node_pad(VuiNode *node, unsigned int top, unsigned int right, unsigned int bottom, unsigned int left)
{
    node->pad_top = top;
    node->pad_right = right;
    node->pad_bottom = bottom;
    node->pad_left = left;
}

static unsigned int sub(unsigned int a, unsigned int b)
{
    return a > b ? a - b : 0;
}

unsigned int node_measure(VuiNode *node, int row)
{
    unsigned int pad = row ? node->pad_left + node->pad_right : node->pad_top + node->pad_bottom, total = 0;
    int along = (node->direction == VUI_DIR_ROW) == (row != 0);

    if (node->measure != NULL) {
        return node->measure(node, row) + pad;
    }

    for (unsigned int i = 0; i < node->child_count; i++) {
        VuiNode *c = node->children[i];
        unsigned int size = along && c->basis != VUI_AUTO ? c->basis : node_measure(c, row);

        total = along ? total + size : (size > total ? size : total);
    }

    if (along && node->child_count > 1) {
        total += node->gap * (node->child_count - 1);
    }

    return total + pad;
}

void node_layout(VuiNode *node, VuiRect rect)
{
    node->rect = rect;

    if (node->child_count == 0) {
        return;
    }

    unsigned int pad_w = node->pad_left + node->pad_right;
    unsigned int pad_h = node->pad_top + node->pad_bottom;
    int inner_x = rect.x + (int)node->pad_left;
    int inner_y = rect.y + (int)node->pad_top;
    unsigned int inner_w = sub(rect.width, pad_w);
    unsigned int inner_h = sub(rect.height, pad_h);

    int row = (node->direction == VUI_DIR_ROW);
    unsigned int main_size = row ? inner_w : inner_h;
    unsigned int cross_size = row ? inner_h : inner_w;

    unsigned int total_gap = node->gap * (node->child_count - 1);
    unsigned int avail = sub(main_size, total_gap);

    unsigned int basis_sum = 0;
    float grow_sum = 0;
    float shrink_basis_sum = 0; // CSS weighs shrink by shrink*basis, not shrink alone

    unsigned int small[8 * 3]; // most containers have a few children: no malloc per node per frame
    unsigned int *sizes = node->child_count <= 8 ? small : malloc(node->child_count * 3 * sizeof(unsigned int));
    unsigned int *bases, *cross;

    if (sizes == NULL) {
        return;
    }

    bases = sizes + node->child_count;
    cross = bases + node->child_count;

    for (unsigned int i = 0; i < node->child_count; i++) {
        VuiNode *c = node->children[i];
        bases[i] = c->basis == VUI_AUTO ? node_measure(c, row) : c->basis;
        basis_sum += bases[i];
        grow_sum += c->grow;
        shrink_basis_sum += c->shrink * (float)bases[i];

        cross[i] = cross_size;

        if (node->align != VUI_ALIGN_STRETCH) {
            unsigned int natural = node_measure(c, !row);

            cross[i] = natural < cross_size ? natural : cross_size;
        }
    }

    // A scrolling container never squeezes its children into the viewport
    if (node->scrolls && basis_sum > avail) {
        node->content = basis_sum + total_gap + (row ? pad_w : pad_h);
        avail = basis_sum;
    } else {
        node->content = main_size + (row ? pad_w : pad_h);
    }

    if (node->scrolls) {
        unsigned int max = sub(node->content, row ? rect.width : rect.height);

        if (node->scroll > max) {
            node->scroll = max;
        }
    }

    int diff = (int)avail - (int)basis_sum;

    unsigned int used = 0;

    for (unsigned int i = 0; i < node->child_count; i++) {
        VuiNode *c = node->children[i];
        unsigned int size = bases[i];

        if (diff > 0 && grow_sum > 0) {
            size += (unsigned int)((float)diff * (c->grow / grow_sum));
        } else if (diff < 0 && shrink_basis_sum > 0) {
            float weight = (c->shrink * (float)bases[i]) / shrink_basis_sum;
            size = sub(size, (unsigned int)((float)(-diff) * weight));
        }

        sizes[i] = size;
        used += size;
    }

    // Float distribution above can leave the row/column a cell or two
    // short of `avail` from rounding -- hand the remainder to the
    // last child so the total always fills exactly. Only when something
    // grows: otherwise the leftover space stays empty (see justify).
    if (used < avail && diff > 0 && grow_sum > 0) {
        sizes[node->child_count - 1] += avail - used;
        used = avail;
    }

    unsigned int free_space = sub(avail, used), gap = node->gap;
    int cursor = (row ? inner_x : inner_y) - (int)node->scroll;

    switch (node->justify) {
    case VUI_JUSTIFY_CENTER: cursor += free_space / 2; break;
    case VUI_JUSTIFY_END:    cursor += free_space; break;
    case VUI_JUSTIFY_BETWEEN:
        if (node->child_count > 1) gap += free_space / (node->child_count - 1);
        break;
    default: break;
    }

    for (unsigned int i = 0; i < node->child_count; i++) {
        VuiNode *c = node->children[i];
        unsigned int spare = cross_size - cross[i];
        int offset = node->align == VUI_ALIGN_CENTER ? spare / 2 : node->align == VUI_ALIGN_END ? spare : 0;
        VuiRect child_rect = row
            ? (VuiRect){ .x = cursor, .y = inner_y + offset, .width = sizes[i], .height = cross[i] }
            : (VuiRect){ .x = inner_x + offset, .y = cursor, .width = cross[i], .height = sizes[i] };

        node_layout(c, child_rect);
        cursor += sizes[i] + gap;
    }

    if (sizes != small) free(sizes);
}

void node_draw(VuiNode *node, VuiScreen *scr)
{
    VuiRect outer = scr_clip(scr, (VuiRect){0});
    int x0 = node->rect.x > outer.x ? node->rect.x : outer.x;
    int y0 = node->rect.y > outer.y ? node->rect.y : outer.y;
    int x1 = node->rect.x + (int)node->rect.width;
    int y1 = node->rect.y + (int)node->rect.height;

    if (x1 > outer.x + (int)outer.width) x1 = outer.x + (int)outer.width;
    if (y1 > outer.y + (int)outer.height) y1 = outer.y + (int)outer.height;

    if (x1 > x0 && y1 > y0) { // fully scrolled-out or clipped-out nodes draw nothing
        scr_clip(scr, (VuiRect){ x0, y0, x1 - x0, y1 - y0 });

        if (node->draw != NULL) {
            node->draw(node, scr, node->userdata);
        }

        for (unsigned int i = 0; i < node->child_count; i++) {
            node_draw(node->children[i], scr);
        }
    }

    scr_clip(scr, outer);
}
