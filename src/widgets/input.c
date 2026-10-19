#include <string.h>

#include "../../include/vaporui.h"
#include "../utf8.h"

static void changed(VuiInput *in)
{
    if (in->on_change != NULL) {
        in->on_change(&in->node);
    }
}

static void insert(VuiInput *in, VuiCodepoint cp)
{
    if (in->len == in->cap) {
        return;
    }

    memmove(in->text + in->cursor + 1, in->text + in->cursor, (in->len - in->cursor) * sizeof(VuiCodepoint));
    in->text[in->cursor++] = cp;
    in->len++;
}

static void erase(VuiInput *in, unsigned int from, unsigned int to)
{
    memmove(in->text + from, in->text + to, (in->len - to) * sizeof(VuiCodepoint));
    in->len -= to - from;
    in->cursor = from;
}

static unsigned int word_left(const VuiInput *in)
{
    unsigned int i = in->cursor;

    while (i > 0 && in->text[i - 1] == ' ') i--;
    while (i > 0 && in->text[i - 1] != ' ') i--;
    return i;
}

static unsigned int word_right(const VuiInput *in)
{
    unsigned int i = in->cursor;

    while (i < in->len && in->text[i] == ' ') i++;
    while (i < in->len && in->text[i] != ' ') i++;
    return i;
}

// Returns 1 if the text changed, 0 if only the cursor moved; -1 if the key isn't ours.
static int edit_key(VuiInput *in, const VuiKeyEvent *k)
{
    VuiModifiers m = k->modifiers;
    int word = m == VUI_MOD_CTRL || m == VUI_MOD_ALT;

    if (m == VUI_MOD_CTRL && k->key == VUI_KEY_CHAR) {
        switch (k->character) {
        case 'a': in->cursor = 0; return 0;
        case 'e': in->cursor = in->len; return 0;
        case 'u': erase(in, 0, in->cursor); return 1;
        case 'k': erase(in, in->cursor, in->len); return 1;
        case 'w': case 'h': erase(in, word_left(in), in->cursor); return 1; // Ctrl-Backspace is often Ctrl-H
        default:  return -1;
        }
    }

    switch (k->key) {
    case VUI_KEY_LEFT:  if (m && !word) return -1; in->cursor = word ? word_left(in) : (in->cursor ? in->cursor - 1 : 0); return 0;
    case VUI_KEY_RIGHT: if (m && !word) return -1; in->cursor = word ? word_right(in) : (in->cursor < in->len ? in->cursor + 1 : in->len); return 0;
    case VUI_KEY_HOME:  in->cursor = 0; return 0;
    case VUI_KEY_END:   in->cursor = in->len; return 0;

    case VUI_KEY_BACKSPACE:
        if (m && !word) return -1;
        erase(in, word ? word_left(in) : (in->cursor ? in->cursor - 1 : 0), in->cursor);
        return 1;

    case VUI_KEY_DELETE:
        if (m && !word) return -1;
        erase(in, in->cursor, word ? word_right(in) : (in->cursor < in->len ? in->cursor + 1 : in->len));
        return 1;

    default:
        return -1;
    }
}

static int input_event(VuiNode *node, VuiCtx *ui, const VuiEvent *event)
{
    VuiInput *in = (VuiInput *)node;
    int result = -1;

    (void)ui;

    switch (event->type) {
    case VUI_EVENT_KEY:
        if (event->key.key == VUI_KEY_ENTER && event->key.modifiers == VUI_MOD_NONE) {
            if (in->on_submit != NULL) {
                in->on_submit(node);
            }
            return 1;
        }

        result = edit_key(in, &event->key);
        break;

    case VUI_EVENT_TEXT:
        // control characters (a pasted newline or tab) don't belong in one line
        if (event->text.codepoint < 32 || event->text.codepoint == 127) {
            return 1;
        }

        insert(in, event->text.codepoint);
        result = 1;
        break;

    case VUI_EVENT_MOUSE:
        if (event->mouse.action == VUI_MOUSE_PRESS && event->mouse.button == VUI_MOUSE_LEFT) {
            unsigned int i = in->left + (event->mouse.x - node->rect.x);

            in->cursor = i < in->len ? i : in->len;
            return 1;
        }
        return 0;

    default:
        return 0;
    }

    if (result == 1) {
        changed(in);
    }

    return result >= 0;
}

static void input_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiInput *in = (VuiInput *)node;
    VuiRect r = node->rect;
    VuiStyle style = { .attr = VUI_ATTR_UNDERLINE | (node->hovered ? VUI_ATTR_BOLD : VUI_ATTR_NONE) };

    (void)userdata;

    if (r.width == 0) {
        return;
    }

    // keep the cursor inside the visible window
    if (in->cursor < in->left) {
        in->left = in->cursor;
    } else if (in->cursor - in->left >= r.width) {
        in->left = in->cursor - r.width + 1;
    }

    scr_fill(scr, (VuiRect){ r.x, r.y, r.width, 1 }, ' ', style);

    for (unsigned int i = 0; i < r.width && in->left + i < in->len; i++) {
        scr_set(scr, r.x + i, r.y, in->text[in->left + i], style);
    }

    if (node->focused) {
        unsigned int c = in->cursor - in->left;

        scr_set(scr, r.x + c, r.y, in->cursor < in->len ? in->text[in->cursor] : ' ', (VuiStyle){ .attr = VUI_ATTR_REVERSE });
    }
}

VuiInput *input_create(unsigned int max_chars)
{
    VuiInput *in = (VuiInput *)node_alloc(sizeof(VuiInput) + max_chars * sizeof(VuiCodepoint));

    if (in != NULL) {
        in->cap = max_chars;
        in->node.basis = 1;
        in->node.focusable = 1;
        in->node.draw = input_draw;
        in->node.on_event = input_event;
    }

    return in;
}

void input_set_text(VuiInput *in, const char *utf8)
{
    VuiUtf8Decoder d;
    uint32_t cp;

    vui_utf8_reset(&d);
    in->len = in->cursor = in->left = 0;

    for (; *utf8 != '\0' && in->len < in->cap; utf8++) {
        if (vui_utf8_feed(&d, (uint8_t)*utf8, &cp) == VUI_UTF8_COMPLETE) {
            in->text[in->len++] = cp;
        }
    }

    in->cursor = in->len;
}

size_t input_get_text(const VuiInput *in, char *out, size_t size)
{
    size_t n = 0;

    for (unsigned int i = 0; i < in->len && size > 0; i++) {
        uint8_t buf[4];
        int len = vui_utf8_encode(in->text[i], buf);

        if (n + len + 1 > size) {
            break;
        }

        memcpy(out + n, buf, len);
        n += len;
    }

    if (size > 0) {
        out[n] = '\0';
    }

    return n;
}
