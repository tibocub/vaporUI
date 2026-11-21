#include "winkeys.h"
#include "scancode.h"
#include "utf8.h"

static VuiModifiers mods_of(unsigned s, int altgr)
{
    VuiModifiers m = VUI_MOD_NONE;

    if (s & VUI_WIN_SHIFT) m |= VUI_MOD_SHIFT;
    if (!altgr && (s & (VUI_WIN_LEFT_ALT | VUI_WIN_RIGHT_ALT))) m |= VUI_MOD_ALT;
    if (!altgr && (s & (VUI_WIN_LEFT_CTRL | VUI_WIN_RIGHT_CTRL))) m |= VUI_MOD_CTRL;
    return m;
}

// Virtual-key codes that are named keys here (winuser.h values)
static VuiKey named_key(unsigned vk)
{
    switch (vk) {
    case 0x08: return VUI_KEY_BACKSPACE;
    case 0x09: return VUI_KEY_TAB;
    case 0x0D: return VUI_KEY_ENTER;
    case 0x1B: return VUI_KEY_ESCAPE;
    case 0x21: return VUI_KEY_PAGE_UP;
    case 0x22: return VUI_KEY_PAGE_DOWN;
    case 0x23: return VUI_KEY_END;
    case 0x24: return VUI_KEY_HOME;
    case 0x25: return VUI_KEY_LEFT;
    case 0x26: return VUI_KEY_UP;
    case 0x27: return VUI_KEY_RIGHT;
    case 0x28: return VUI_KEY_DOWN;
    case 0x2D: return VUI_KEY_INSERT;
    case 0x2E: return VUI_KEY_DELETE;
    default:
        if (vk >= 0x70 && vk <= 0x7B) return (VuiKey)(VUI_KEY_F1 + (vk - 0x70)); // F1..F12
        return VUI_KEY_NONE;
    }
}

static int is_modifier_vk(unsigned vk)
{
    return vk == 0x10 || vk == 0x11 || vk == 0x12 || vk == 0x14 || vk == 0x5B || vk == 0x5C || vk == 0x90 || vk == 0x91;
}

int vui_winkey_state(unsigned vk, unsigned scan, int down, uint32_t unshifted, unsigned ctrl_state, VuiEvent *out)
{
    VuiScancode sc = vui_scancode_from_set1(scan, (ctrl_state & VUI_WIN_ENHANCED) != 0);
    VuiKey named = named_key(vk);

    if (vk == 0 || vk == 0xFF || (down && !is_modifier_vk(vk))) return 0;

    *out = (VuiEvent){ .type = VUI_EVENT_KEY };
    out->key.scancode = sc;
    out->key.modifiers = mods_of(ctrl_state, 0);
    out->key.release = !down;

    if (named != VUI_KEY_NONE) {
        out->key.key = named;
    } else if (!is_modifier_vk(vk)) {
        out->key.key = VUI_KEY_CHAR;
        out->key.character = unshifted != 0 ? vui_cp_lower_of(unshifted) ? vui_cp_lower_of(unshifted) : unshifted : vui_scancode_base(sc);
        out->key.base = vui_scancode_base(sc) != 0 ? vui_scancode_base(sc) : out->key.character;
    }

    return 1;
}

int vui_winkey_translate(unsigned vk, unsigned scan, uint32_t uc, uint32_t unshifted, unsigned ctrl_state, VuiEvent *out)
{
    VuiScancode sc = vui_scancode_from_set1(scan, (ctrl_state & VUI_WIN_ENHANCED) != 0);
    int altgr = (ctrl_state & VUI_WIN_RIGHT_ALT) && (ctrl_state & VUI_WIN_LEFT_CTRL) && uc >= 0x20;
    VuiModifiers mods = mods_of(ctrl_state, altgr);
    VuiKey named = named_key(vk);

    // Shift, Ctrl, Alt, the Windows keys, Caps/Num/Scroll Lock, and the unassigned codes: not keys for the app
    if (vk == 0x10 || vk == 0x11 || vk == 0x12 || vk == 0x14 || vk == 0x5B || vk == 0x5C || vk == 0x90 || vk == 0x91
        || vk == 0 || vk == 0xFF) {
        return 0;
    }

    *out = (VuiEvent){ .type = VUI_EVENT_KEY };
    out->key.scancode = sc;
    out->key.modifiers = mods;

    if (named != VUI_KEY_NONE) {
        out->key.key = named;
        return 1;
    }

    if (!(mods & (VUI_MOD_CTRL | VUI_MOD_ALT))) {
        // typing: a printable character is text, anything else (a dead key waiting for its partner) is nothing
        if (uc < 0x20 || uc == 0x7F) return 0;

        *out = (VuiEvent){ .type = VUI_EVENT_TEXT };
        out->text.codepoint = uc;
        return 1;
    }

    // Ctrl or Alt held: the key and its modifiers. Windows hands Ctrl+letter over as a control code, so the
    // character is what the key types alone on this layout.
    out->key.key = VUI_KEY_CHAR;
    out->key.character = unshifted != 0 ? unshifted : (uc >= 0x20 ? uc : vui_scancode_base(sc));
    if (vui_cp_lower_of(out->key.character) != 0) out->key.character = vui_cp_lower_of(out->key.character);
    out->key.base = vui_scancode_base(sc) != 0 ? vui_scancode_base(sc) : out->key.character;
    return out->key.character != 0;
}

int vui_winmouse_translate(unsigned buttons, unsigned prev_buttons, unsigned flags, unsigned x, unsigned y, unsigned ctrl_state, int level, VuiEvent *out)
{
    enum { MOVED = 1, WHEELED = 4, HWHEELED = 8 };
    VuiMouseEvent *m = &out->mouse;
    unsigned changed = (buttons ^ prev_buttons) & 7u;
    int wheel = (int)(short)(buttons >> 16);

    if (level <= 0) return 0;

    *out = (VuiEvent){ .type = VUI_EVENT_MOUSE };
    m->x = x;
    m->y = y;
    m->modifiers = mods_of(ctrl_state, 0);

    if (flags & (WHEELED | HWHEELED)) {
        m->action = VUI_MOUSE_PRESS;
        if (flags & WHEELED) m->button = wheel > 0 ? VUI_MOUSE_WHEEL_UP : VUI_MOUSE_WHEEL_DOWN;
        else m->button = wheel > 0 ? VUI_MOUSE_WHEEL_RIGHT : VUI_MOUSE_WHEEL_LEFT;
        return 1;
    }

    if (changed != 0) {
        unsigned bit = changed & -changed; // one change per record

        m->button = bit == 1 ? VUI_MOUSE_LEFT : bit == 2 ? VUI_MOUSE_RIGHT : VUI_MOUSE_MIDDLE;
        m->action = (buttons & bit) ? VUI_MOUSE_PRESS : VUI_MOUSE_RELEASE;
        return 1;
    }

    if (flags & MOVED) {
        m->button = (buttons & 1) ? VUI_MOUSE_LEFT : (buttons & 2) ? VUI_MOUSE_RIGHT : (buttons & 4) ? VUI_MOUSE_MIDDLE : VUI_MOUSE_NONE;
        m->action = VUI_MOUSE_MOVE;
        return m->button != VUI_MOUSE_NONE || level >= 2;
    }

    return 0;
}
