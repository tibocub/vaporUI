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
    decoder->index = 0;
    decoder->sub = 0;
}

static VuiVtResult done(VuiKeyEvent *event, VuiKey key, int mod)
{
    event->key = key;
    event->modifiers = mod > 1 ? (VuiModifiers)((mod - 1) & 15) : VUI_MOD_NONE;
    event->character = 0;
    return VUI_VT_COMPLETE;
}

VuiVtResult vui_vt_feed(VuiVtDecoder *decoder, uint8_t byte, VuiKeyEvent *event)
{
    int mod = decoder->param[1];

    if (decoder->state == VUI_VT_STATE_INTRO) {
        if (byte != '[' && byte != 'O') {
            return VUI_VT_INVALID;
        }

        decoder->state = VUI_VT_STATE_PARAM;
        return VUI_VT_NEED_MORE;
    }

    if (byte >= '0' && byte <= '9') {
        // shortcut: only the first two params are kept, rest are dropped
        if (!decoder->sub && decoder->index < 2) {
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

    switch (byte) {
    case 'A': return done(event, VUI_KEY_UP, mod);
    case 'B': return done(event, VUI_KEY_DOWN, mod);
    case 'C': return done(event, VUI_KEY_RIGHT, mod);
    case 'D': return done(event, VUI_KEY_LEFT, mod);
    case 'H': return done(event, VUI_KEY_HOME, mod);
    case 'F': return done(event, VUI_KEY_END, mod);
    case 'Z': return done(event, VUI_KEY_TAB, 2); // Shift+Tab

    // Numeric-param sequences (CSI <n> ~), used for the keys that
    // don't have their own final letter.
    case '~':
        switch (decoder->param[0]) {
        case 1: case 7: return done(event, VUI_KEY_HOME, mod);
        case 3:         return done(event, VUI_KEY_DELETE, mod);
        case 4: case 8: return done(event, VUI_KEY_END, mod);
        case 5:         return done(event, VUI_KEY_PAGE_UP, mod);
        case 6:         return done(event, VUI_KEY_PAGE_DOWN, mod);
        default:        return VUI_VT_INVALID;
        }

    case 'u':
        switch (decoder->param[0]) {
        case 9:   return done(event, VUI_KEY_TAB, mod);
        case 13:  return done(event, VUI_KEY_ENTER, mod);
        case 27:  return done(event, VUI_KEY_ESCAPE, mod);
        case 127: return done(event, VUI_KEY_BACKSPACE, mod);
        case 0:   return VUI_VT_INVALID;
        default:
            done(event, VUI_KEY_CHAR, mod);
            event->character = decoder->param[0];
            return VUI_VT_COMPLETE;
        }

    default:
        return VUI_VT_INVALID;
    }
}
