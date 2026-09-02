#include "input.h"

void vui_input_reset(VuiInputDecoder *decoder)
{
    decoder->in_escape = 0;
    vui_utf8_reset(&decoder->utf8);
}

static VuiEvent key_event(VuiKey key)
{
    VuiEvent event = { .type = VUI_EVENT_KEY };

    event.key.key = key;
    event.key.modifiers = VUI_MOD_NONE;

    return event;
}

VuiEvent vui_input_feed(VuiInputDecoder *decoder, uint8_t byte)
{
    VuiEvent event = { .type = VUI_EVENT_NONE };

    if (decoder->in_escape) {
        VuiKey key;

        switch (vui_vt_feed(&decoder->vt, byte, &key)) {
        case VUI_VT_NEED_MORE:
            return event;

        case VUI_VT_COMPLETE:
            decoder->in_escape = 0;
            return key_event(key);

        case VUI_VT_INVALID:
            decoder->in_escape = 0;
            return event;
        }
    }

    // NOTE: a real lone Escape keypress also starts here and never
    // gets a '[' to complete the sequence, so it'll sit in
    // in_escape until the *next* byte arrives -- which then gets
    // wrongly fed to the VT decoder instead of handled on its own.
    // Telling the two apart needs a short read timeout at the
    // platform layer once we're mid-escape; not done yet.
    if (byte == 0x1B) {
        decoder->in_escape = 1;
        vui_vt_reset(&decoder->vt);
        return event;
    }

    switch (byte) {
    case 3:    return key_event(VUI_KEY_CTRL_C);
    case '\n':
    case '\r': return key_event(VUI_KEY_ENTER);
    case '\t': return key_event(VUI_KEY_TAB);
    case 127:  return key_event(VUI_KEY_BACKSPACE);
    default:   break;
    }

    {
        uint32_t codepoint;

        if (vui_utf8_feed(&decoder->utf8, byte, &codepoint) == VUI_UTF8_COMPLETE) {
            event.type = VUI_EVENT_TEXT;
            event.text.codepoint = codepoint;
        }
    }

    return event;
}
