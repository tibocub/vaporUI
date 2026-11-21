#include "scancode.h"

// US-layout character of each key in the HID range 4..100 (the typing keys), 0 where a key types nothing
static const char us_base[101] = {
    [VUI_SC_A] = 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    [VUI_SC_1] = '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    [VUI_SC_SPACE] = ' ', '-', '=', '[', ']', '\\',
    [VUI_SC_SEMICOLON] = ';', '\'', '`', ',', '.', '/',
};

VuiCodepoint vui_scancode_base(VuiScancode sc)
{
    return sc > 0 && sc < (int)sizeof(us_base) ? (VuiCodepoint)us_base[sc] : 0;
}

VuiScancode vui_scancode_from_base(VuiCodepoint base)
{
    for (int sc = 4; sc < (int)sizeof(us_base); sc++) {
        if (us_base[sc] != 0 && (VuiCodepoint)us_base[sc] == base) return (VuiScancode)sc;
    }

    return VUI_SC_NONE;
}

// PC scancode set 1 (what Windows reports as wVirtualScanCode; `extended` is the E0 prefix, ENHANCED_KEY)
VuiScancode vui_scancode_from_set1(unsigned int sc, int extended)
{
    static const unsigned char main_block[0x59] = {
        [0x01] = VUI_SC_ESCAPE, [0x02] = VUI_SC_1, VUI_SC_2, VUI_SC_3, VUI_SC_4, VUI_SC_5, VUI_SC_6, VUI_SC_7,
        VUI_SC_8, VUI_SC_9, VUI_SC_0, VUI_SC_MINUS, VUI_SC_EQUALS, VUI_SC_BACKSPACE, VUI_SC_TAB,
        [0x10] = VUI_SC_Q, VUI_SC_W, VUI_SC_E, VUI_SC_R, VUI_SC_T, VUI_SC_Y, VUI_SC_U, VUI_SC_I, VUI_SC_O, VUI_SC_P,
        VUI_SC_LBRACKET, VUI_SC_RBRACKET, VUI_SC_ENTER, VUI_SC_LCTRL,
        [0x1E] = VUI_SC_A, VUI_SC_S, VUI_SC_D, VUI_SC_F, VUI_SC_G, VUI_SC_H, VUI_SC_J, VUI_SC_K, VUI_SC_L,
        VUI_SC_SEMICOLON, VUI_SC_APOSTROPHE, VUI_SC_GRAVE, VUI_SC_LSHIFT, VUI_SC_BACKSLASH,
        VUI_SC_Z, VUI_SC_X, VUI_SC_C, VUI_SC_V, VUI_SC_B, VUI_SC_N, VUI_SC_M, VUI_SC_COMMA, VUI_SC_PERIOD,
        VUI_SC_SLASH, VUI_SC_RSHIFT, VUI_SC_KP_MULTIPLY, VUI_SC_LALT, VUI_SC_SPACE, VUI_SC_CAPSLOCK,
        VUI_SC_F1, VUI_SC_F2, VUI_SC_F3, VUI_SC_F4, VUI_SC_F5, VUI_SC_F6, VUI_SC_F7, VUI_SC_F8, VUI_SC_F9, VUI_SC_F10,
        VUI_SC_NUMLOCK, VUI_SC_SCROLLLOCK, VUI_SC_KP_7, VUI_SC_KP_8, VUI_SC_KP_9, VUI_SC_KP_MINUS,
        VUI_SC_KP_4, VUI_SC_KP_5, VUI_SC_KP_6, VUI_SC_KP_PLUS, VUI_SC_KP_1, VUI_SC_KP_2, VUI_SC_KP_3,
        VUI_SC_KP_0, VUI_SC_KP_PERIOD,
        [0x56] = VUI_SC_NONUS_BACKSLASH, VUI_SC_F11, VUI_SC_F12,
    };

    if (!extended) return sc < sizeof(main_block) ? (VuiScancode)main_block[sc] : VUI_SC_NONE;

    switch (sc) {
    case 0x1C: return VUI_SC_KP_ENTER;
    case 0x1D: return VUI_SC_RCTRL;
    case 0x35: return VUI_SC_KP_DIVIDE;
    case 0x37: return VUI_SC_PRINTSCREEN;
    case 0x38: return VUI_SC_RALT;
    case 0x47: return VUI_SC_HOME;
    case 0x48: return VUI_SC_UP;
    case 0x49: return VUI_SC_PAGEUP;
    case 0x4B: return VUI_SC_LEFT;
    case 0x4D: return VUI_SC_RIGHT;
    case 0x4F: return VUI_SC_END;
    case 0x50: return VUI_SC_DOWN;
    case 0x51: return VUI_SC_PAGEDOWN;
    case 0x52: return VUI_SC_INSERT;
    case 0x53: return VUI_SC_DELETE;
    case 0x5B: return VUI_SC_LGUI;
    case 0x5C: return VUI_SC_RGUI;
    case 0x5D: return VUI_SC_APPLICATION;
    default:   return VUI_SC_NONE;
    }
}
