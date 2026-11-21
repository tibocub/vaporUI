#include <stdlib.h>
#include <string.h>

#include "input.h"

static VuiShiftProbe shift_probe;

void vui_input_set_shift_probe(VuiShiftProbe probe)
{
    shift_probe = probe;
}

void vui_input_reset(VuiInputDecoder *decoder)
{
    decoder->shift_state = -1;
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

static VuiEvent feed(VuiInputDecoder *decoder, uint8_t byte)
{
    VuiEvent event = { .type = VUI_EVENT_NONE };

    if (decoder->in_escape) {
        // ESC followed by anything but '[' / 'O' is Alt+<that key>
        if (decoder->vt.state == VUI_VT_STATE_INTRO && byte != '[' && byte != 'O') {
            decoder->in_escape = 0;
            decoder->alt = 1;
            decoder->shift_state = shift_probe != NULL ? shift_probe() : -1;
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

// -- Normalising letters and finding the physical key --

// Simple lowercase of the alphabets that have it: Latin (with accents), Greek, Cyrillic. 0 if `c` isn't uppercase.
static uint32_t lower_of(uint32_t c)
{
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
    if (c == 0x178) return 0xFF;
    if ((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177)) return (c & 1) ? 0 : c + 1;
    if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) return (c & 1) ? c + 1 : 0;
    if (c >= 0x391 && c <= 0x3A9 && c != 0x3A2) return c + 32;
    if (c >= 0x410 && c <= 0x42F) return c + 32;
    if (c >= 0x400 && c <= 0x40F) return c + 80;
    return 0;
}

// What a layout types, as the key it is on a US keyboard: typed character -> base key, and whether Shift is held
// for that character. Characters not listed are their own base key (letters, and digits on US).
typedef struct { uint32_t typed, base; uint8_t shift; } LayoutKey;

static const LayoutKey us_keys[] = {
    { '!', '1', 1 }, { '@', '2', 1 }, { '#', '3', 1 }, { '$', '4', 1 }, { '%', '5', 1 }, { '^', '6', 1 },
    { '&', '7', 1 }, { '*', '8', 1 }, { '(', '9', 1 }, { ')', '0', 1 }, { '_', '-', 1 }, { '+', '=', 1 },
    { '{', '[', 1 }, { '}', ']', 1 }, { '|', '\\', 1 }, { ':', ';', 1 }, { '"', '\'', 1 }, { '<', ',', 1 },
    { '>', '.', 1 }, { '?', '/', 1 }, { '~', '`', 1 },
    { 0, 0, 0 }
};

static const LayoutKey fr_keys[] = {
    { '&', '1', 0 }, { 0xE9, '2', 0 }, { '"', '3', 0 }, { '\'', '4', 0 }, { '(', '5', 0 }, { '-', '6', 0 },
    { 0xE8, '7', 0 }, { '_', '8', 0 }, { 0xE7, '9', 0 }, { 0xE0, '0', 0 }, { ')', '-', 0 }, { '=', '=', 0 },
    { '1', '1', 1 }, { '2', '2', 1 }, { '3', '3', 1 }, { '4', '4', 1 }, { '5', '5', 1 }, { '6', '6', 1 },
    { '7', '7', 1 }, { '8', '8', 1 }, { '9', '9', 1 }, { '0', '0', 1 }, { 0xB0, '-', 1 }, { '+', '=', 1 },
    { '^', '[', 0 }, { '$', ']', 0 }, { 0xF9, '\'', 0 }, { '*', '\\', 0 },
    { 0xA8, '[', 1 }, { 0xA3, ']', 1 }, { '%', '\'', 1 }, { 0xB5, '\\', 1 },
    { 'a', 'q', 0 }, { 'q', 'a', 0 }, { 'z', 'w', 0 }, { 'w', 'z', 0 },
    { 'm', ';', 0 }, { ',', 'm', 0 }, { ';', ',', 0 }, { ':', '.', 0 }, { '!', '/', 0 },
    { '?', 'm', 1 }, { '.', ',', 1 }, { '/', '.', 1 }, { 0xA7, '/', 1 },
    { 0, 0, 0 }
};

static const LayoutKey *layout = us_keys;
static int layout_chosen;

int vui_set_layout(const char *name)
{
    if (name == NULL || *name == '\0') name = getenv("VUI_LAYOUT");

    if (name == NULL || *name == '\0' || !strcmp(name, "us")) layout = us_keys;
    else if (!strcmp(name, "fr")) layout = fr_keys;
    else return -1;

    layout_chosen = 1;
    return 0;
}

// A letter key is always reported as the lowercase letter plus VUI_MOD_SHIFT, whatever the terminal sent: a legacy
// terminal says Alt+Shift+q as ESC 'Q' (no Shift), a kitty / modifyOtherKeys one as 'q' or 'Q' with Shift.
// Then the base key: from the terminal if it said (kitty alternate keys), else from the layout table.
VuiEvent vui_input_feed(VuiInputDecoder *decoder, uint8_t byte)
{
    VuiEvent e = feed(decoder, byte);

    if (e.type == VUI_EVENT_KEY && e.key.key == VUI_KEY_CHAR) {
        VuiKeyEvent *k = &e.key;
        uint32_t lc = lower_of(k->character);

        if (lc != 0) {
            k->character = lc;
            k->modifiers |= VUI_MOD_SHIFT;
        }

        if (decoder->shift_state >= 0) {
            // the console told us whether Shift is down: that is the truth, not what the typed character implies
            k->modifiers = (VuiModifiers)((k->modifiers & ~VUI_MOD_SHIFT) | (decoder->shift_state ? VUI_MOD_SHIFT : 0));
        }

        if (k->base == 0) {
            if (!layout_chosen) vui_set_layout(NULL);

            k->base = k->character;
            for (const LayoutKey *l = layout; l->typed != 0; l++) {
                if (l->typed == k->character) {
                    k->base = l->base;
                    if (l->shift && decoder->shift_state < 0) k->modifiers |= VUI_MOD_SHIFT;
                    break;
                }
            }
        }
    }

    if (e.type != VUI_EVENT_NONE) decoder->shift_state = -1; // consumed by this event, whatever it was

    return e;
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
