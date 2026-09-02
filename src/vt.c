#include "vt.h"

// Only CSI sequences (ESC '[' ...) are handled. SS3 sequences
// (ESC 'O' ...), which some terminals use for arrow/function keys in
// application-cursor-keys mode, aren't decoded yet.

void vui_vt_reset(VuiVtDecoder *decoder)
{
    decoder->state = VUI_VT_STATE_CSI;
    decoder->param = 0;
}

VuiVtResult vui_vt_feed(VuiVtDecoder *decoder, uint8_t byte, VuiKey *key)
{
    if (decoder->state == VUI_VT_STATE_CSI) {
        if (byte != '[') {
            return VUI_VT_INVALID;
        }

        decoder->state = VUI_VT_STATE_PARAM;
        return VUI_VT_NEED_MORE;
    }

    if (byte >= '0' && byte <= '9') {
        decoder->param = decoder->param * 10 + (byte - '0');
        return VUI_VT_NEED_MORE;
    }

    switch (byte) {
    case 'A': *key = VUI_KEY_UP;    return VUI_VT_COMPLETE;
    case 'B': *key = VUI_KEY_DOWN;  return VUI_VT_COMPLETE;
    case 'C': *key = VUI_KEY_RIGHT; return VUI_VT_COMPLETE;
    case 'D': *key = VUI_KEY_LEFT;  return VUI_VT_COMPLETE;
    case 'H': *key = VUI_KEY_HOME;  return VUI_VT_COMPLETE;
    case 'F': *key = VUI_KEY_END;   return VUI_VT_COMPLETE;

    // Numeric-param sequences (CSI <n> '~'), used for the keys that
    // don't have their own final letter.
    case '~':
        switch (decoder->param) {
        case 1: case 7: *key = VUI_KEY_HOME;      return VUI_VT_COMPLETE;
        case 3:         *key = VUI_KEY_DELETE;    return VUI_VT_COMPLETE;
        case 4: case 8: *key = VUI_KEY_END;       return VUI_VT_COMPLETE;
        case 5:         *key = VUI_KEY_PAGE_UP;   return VUI_VT_COMPLETE;
        case 6:         *key = VUI_KEY_PAGE_DOWN; return VUI_VT_COMPLETE;
        default:        return VUI_VT_INVALID;
        }

    default:
        return VUI_VT_INVALID;
    }
}
