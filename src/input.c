#include "input.h"

void vui_input_reset(VuiInputDecoder *decoder)
{
    decoder->in_escape = 0;
    decoder->alt = 0;
    decoder->in_paste = 0;
    vui_utf8_reset(&decoder->utf8);
}

static VuiEvent key_event(VuiKey key)
{
    VuiEvent event = { .type = VUI_EVENT_KEY };

    event.key.key = key;
    event.key.modifiers = VUI_MOD_NONE;
    event.key.character = 0;

    return event;
}

static VuiEvent char_key_event(VuiCodepoint character, VuiModifiers modifiers)
{
    VuiEvent event = { .type = VUI_EVENT_KEY };

    event.key.key = VUI_KEY_CHAR;
    event.key.modifiers = modifiers;
    event.key.character = character;

    return event;
}

static VuiEvent decode_byte(VuiInputDecoder *decoder, uint8_t byte)
{
    VuiEvent event = { .type = VUI_EVENT_NONE };

    // Pasted bytes are plain text, control characters included
    if (decoder->in_paste) {
        uint32_t codepoint;

        if (byte == '\r') {
            byte = '\n';
        }

        if (vui_utf8_feed(&decoder->utf8, byte, &codepoint) == VUI_UTF8_COMPLETE) {
            event.type = VUI_EVENT_TEXT;
            event.text.codepoint = codepoint;
        }

        return event;
    }

    switch (byte) {
    case '\n':
    case '\r': return key_event(VUI_KEY_ENTER);
    case '\t': return key_event(VUI_KEY_TAB);
    case 127:  return key_event(VUI_KEY_BACKSPACE);
    case 0x1B: return key_event(VUI_KEY_ESCAPE); // only reached as Alt+Escape
    default:   break;
    }

    // Remaining C0 control bytes are Ctrl+<letter> -- terminals send
    // these by clearing bit 6 of the letter's ASCII code, so byte 1
    // is Ctrl-A, byte 26 is Ctrl-Z, etc.
    if (byte >= 1 && byte <= 26) {
        return char_key_event('a' + (byte - 1), VUI_MOD_CTRL);
    }

    // The rest of the C0 range: NUL is Ctrl+Space, 28-31 are Ctrl+\ ] ^ _
    if (byte == 0) {
        return char_key_event(' ', VUI_MOD_CTRL);
    }

    if (byte >= 28 && byte <= 31) {
        return char_key_event(byte + 64, VUI_MOD_CTRL);
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

VuiEvent vui_input_feed(VuiInputDecoder *decoder, uint8_t byte)
{
    VuiEvent event = { .type = VUI_EVENT_NONE };

    if (decoder->in_escape) {
        // ESC followed by anything but '[' / 'O' is Alt+<that key>
        if (decoder->vt.state == VUI_VT_STATE_INTRO && byte != '[' && byte != 'O') {
            decoder->in_escape = 0;
            decoder->alt = 1;
        } else {
            VuiEvent parsed;

            switch (vui_vt_feed(&decoder->vt, byte, &parsed)) {
            case VUI_VT_NEED_MORE:
                return event;

            case VUI_VT_COMPLETE:
                decoder->in_escape = 0;

                if (parsed.type == VUI_EVENT_PASTE_START) {
                    decoder->in_paste = 1;
                } else if (parsed.type == VUI_EVENT_PASTE_END) {
                    decoder->in_paste = 0;
                } else if (decoder->in_paste) {
                    return event; // stray sequence inside a paste
                }

                return parsed;

            case VUI_VT_INVALID:
                decoder->in_escape = 0;
                return event;
            }
        }
    }

    if (byte == 0x1B && !decoder->alt) {
        decoder->in_escape = 1;
        vui_vt_reset(&decoder->vt);
        return event;
    }

    event = decode_byte(decoder, byte);

    if (decoder->alt && event.type != VUI_EVENT_NONE) {
        decoder->alt = 0;

        // Alt+text is a key, not text to insert
        if (event.type == VUI_EVENT_TEXT) {
            event = char_key_event(event.text.codepoint, VUI_MOD_ALT);
        } else {
            event.key.modifiers |= VUI_MOD_ALT;
        }
    }

    return event;
}

int vui_input_pending_escape(const VuiInputDecoder *decoder)
{
    return decoder->in_escape;
}

VuiEvent vui_input_timeout(VuiInputDecoder *decoder)
{
    if (!decoder->in_escape) {
        return (VuiEvent){ .type = VUI_EVENT_NONE };
    }

    decoder->in_escape = 0;
    return key_event(VUI_KEY_ESCAPE);
}
