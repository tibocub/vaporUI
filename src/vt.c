#include "vt.h"

// Decodes CSI (ESC '[') and SS3 (ESC 'O') key sequences, including the
// xterm modifier form (CSI 1;<mod> A) and the kitty/fixterms form
// (CSI <codepoint>;<mod> u). <mod> is 1 + a bitmask whose low four bits
// match VuiModifiers.

void vui_vt_reset(VuiVtDecoder *decoder)
{
    decoder->state = VUI_VT_STATE_INTRO;
    decoder->param[0] = 0;
    decoder->param[1] = 0;
    decoder->param[2] = 0;
    decoder->mouse = 0;
    decoder->index = 0;
    decoder->sub = 0;
}

static VuiVtResult done(VuiEvent *event, VuiKey key, int mod)
{
    event->type = VUI_EVENT_KEY;
    event->key.key = key;
    event->key.modifiers = mod > 1 ? (VuiModifiers)((mod - 1) & 15) : VUI_MOD_NONE;
    event->key.character = 0;
    return VUI_VT_COMPLETE;
}

// Shared by the kitty (CSI code;mod u) and xterm modifyOtherKeys
// (CSI 27;mod;code ~) forms.
static VuiVtResult other_key(VuiEvent *event, int code, int mod)
{
    switch (code) {
    case 0:   return VUI_VT_INVALID;
    case 9:   return done(event, VUI_KEY_TAB, mod);
    case 13:  return done(event, VUI_KEY_ENTER, mod);
    case 27:  return done(event, VUI_KEY_ESCAPE, mod);
    case 127: return done(event, VUI_KEY_BACKSPACE, mod);
    default:
        done(event, VUI_KEY_CHAR, mod);
        event->key.character = code;
        return VUI_VT_COMPLETE;
    }
}

static VuiVtResult marker(VuiEvent *event, VuiEventType type)
{
    event->type = type;
    return VUI_VT_COMPLETE;
}

// SGR mouse: CSI < b ; x ; y (M press | m release). b's low 2 bits are
// the button, +4 shift, +8 alt, +16 ctrl, +32 motion, +64 wheel.
static VuiVtResult mouse(VuiVtDecoder *decoder, uint8_t final, VuiEvent *event)
{
    static const VuiMouseButton buttons[] = { VUI_MOUSE_LEFT, VUI_MOUSE_MIDDLE, VUI_MOUSE_RIGHT, VUI_MOUSE_NONE };
    int b = decoder->param[0];

    if (decoder->param[1] < 1 || decoder->param[2] < 1) {
        return VUI_VT_INVALID;
    }

    event->type = VUI_EVENT_MOUSE;
    event->mouse.button = (b & 64) ? VUI_MOUSE_WHEEL_UP + (b & 3) : buttons[b & 3];
    event->mouse.action = (b & 32) ? VUI_MOUSE_MOVE : final == 'M' ? VUI_MOUSE_PRESS : VUI_MOUSE_RELEASE;
    event->mouse.modifiers = ((b & 4) ? VUI_MOD_SHIFT : 0) | ((b & 8) ? VUI_MOD_ALT : 0) | ((b & 16) ? VUI_MOD_CTRL : 0);
    event->mouse.x = decoder->param[1] - 1;
    event->mouse.y = decoder->param[2] - 1;
    return VUI_VT_COMPLETE;
}

VuiVtResult vui_vt_feed(VuiVtDecoder *decoder, uint8_t byte, VuiEvent *event)
{
    int mod = decoder->param[1];

    if (decoder->state == VUI_VT_STATE_INTRO) {
        if (byte != '[' && byte != 'O') {
            return VUI_VT_INVALID;
        }

        decoder->state = VUI_VT_STATE_PARAM;
        return VUI_VT_NEED_MORE;
    }

    if (byte == '<' && decoder->index == 0 && decoder->param[0] == 0) {
        decoder->mouse = 1;
        return VUI_VT_NEED_MORE;
    }

    if (byte >= '0' && byte <= '9') {
        // shortcut: only the first three params are kept, rest are dropped
        if (!decoder->sub && decoder->index < 3) {
            decoder->param[decoder->index] = decoder->param[decoder->index] * 10 + (byte - '0');
        }
        return VUI_VT_NEED_MORE;
    }

    if (byte == ';') {
        decoder->index++;
        decoder->sub = 0;
        return VUI_VT_NEED_MORE;
    }

    // ':' starts kitty sub-fields (alternate keys, event type): ignored
    if (byte == ':') {
        decoder->sub = 1;
        return VUI_VT_NEED_MORE;
    }

    if (decoder->mouse) {
        return (byte == 'M' || byte == 'm') ? mouse(decoder, byte, event) : VUI_VT_INVALID;
    }

    switch (byte) {
    case 'A': return done(event, VUI_KEY_UP, mod);
    case 'B': return done(event, VUI_KEY_DOWN, mod);
    case 'C': return done(event, VUI_KEY_RIGHT, mod);
    case 'D': return done(event, VUI_KEY_LEFT, mod);
    case 'H': return done(event, VUI_KEY_HOME, mod);
    case 'F': return done(event, VUI_KEY_END, mod);
    case 'P': return done(event, VUI_KEY_F1, mod);
    case 'Q': return done(event, VUI_KEY_F2, mod);
    case 'R': return done(event, VUI_KEY_F3, mod);
    case 'S': return done(event, VUI_KEY_F4, mod);
    case 'Z': return done(event, VUI_KEY_TAB, 2); // Shift+Tab

    // Numeric-param sequences (CSI <n> ~), used for the keys that
    // don't have their own final letter.
    case '~':
        switch (decoder->param[0]) {
        case 1: case 7: return done(event, VUI_KEY_HOME, mod);
        case 2:         return done(event, VUI_KEY_INSERT, mod);
        case 27:        return other_key(event, decoder->param[2], mod);
        case 200:       return marker(event, VUI_EVENT_PASTE_START);
        case 201:       return marker(event, VUI_EVENT_PASTE_END);
        case 3:         return done(event, VUI_KEY_DELETE, mod);
        case 4: case 8: return done(event, VUI_KEY_END, mod);
        case 5:         return done(event, VUI_KEY_PAGE_UP, mod);
        case 6:         return done(event, VUI_KEY_PAGE_DOWN, mod);
        case 11: case 12: case 13: case 14: case 15:
            return done(event, VUI_KEY_F1 + decoder->param[0] - 11, mod);
        case 17: case 18: case 19: case 20: case 21:
            return done(event, VUI_KEY_F6 + decoder->param[0] - 17, mod);
        case 23: case 24:
            return done(event, VUI_KEY_F11 + decoder->param[0] - 23, mod);
        default:        return VUI_VT_INVALID;
        }

    case 'u': return other_key(event, decoder->param[0], mod);

    default:
        return VUI_VT_INVALID;
    }
}
