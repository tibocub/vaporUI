#include <string.h>

#include "vtraw.h"
#include "scancode.h"

enum { KT_LATIN, KT_FN, KT_SPEC, KT_PAD, KT_DEAD, KT_CONS, KT_CUR, KT_SHIFT, KT_META, KT_ASCII, KT_LOCK, KT_LETTER };
enum { KG_SHIFT, KG_ALTGR, KG_CTRL, KG_ALT, KG_SHIFTL, KG_SHIFTR, KG_CTRLL, KG_CTRLR, KG_CAPSSHIFT };
enum { K_ENTER = 1, K_CAPS = 7, K_NUM = 8 };

VuiVtEntry vui_vtraw_entry(unsigned int v)
{
    static const uint8_t dead[] = { '`', '\'', '^', '~', '"', ',' }; // the kernel's obsolete KT_DEAD 0..5
    unsigned int type = v >> 8, val = v & 0xff;
    VuiVtEntry e = { VT_NONE, 0 };

    if (type >= 16) { // a Unicode character, stored xor 0xF000
        e.type = VT_CHAR;
        e.value = v ^ 0xF000u;
        return e;
    }

    e.value = val;
    switch (type) {
    case KT_LATIN:  e.type = VT_CHAR; break;
    case KT_LETTER: e.type = VT_LETTER; break;
    case KT_FN:     e.type = VT_FN; break;
    case KT_CUR:    e.type = VT_CUR; break;
    case KT_PAD:    e.type = VT_PAD; break;
    case KT_SPEC:   e.type = VT_SPEC; break;
    case KT_SHIFT:  e.type = VT_SHIFT; break;
    case KT_DEAD:
        if (val < sizeof(dead)) { e.type = VT_DEAD; e.value = dead[val]; }
        break;
    default: break;
    }

    return e;
}

void vui_vtraw_init(VuiVtRaw *raw, int numlock)
{
    memset(raw->down, 0, sizeof(raw->down));
    raw->caps = 0;
    raw->num = numlock != 0;
    raw->dead = 0;
    raw->pending = 0;
    raw->queue_len = 0;
    raw->switch_to = 0;
}

void vui_vtraw_release_all(VuiVtRaw *raw)
{
    memset(raw->down, 0, sizeof(raw->down));
    raw->dead = 0;
    raw->pending = 0;
}

static void push(VuiVtRaw *raw, VuiEvent e)
{
    if (raw->queue_len < 3) raw->queue[raw->queue_len++] = e;
}

static void push_text(VuiVtRaw *raw, uint32_t cp)
{
    VuiEvent e = { .type = VUI_EVENT_TEXT };

    e.text.codepoint = cp;
    push(raw, e);
}

VuiEvent vui_vtraw_pop(VuiVtRaw *raw)
{
    VuiEvent e = { .type = VUI_EVENT_NONE };

    if (raw->queue_len > 0) {
        e = raw->queue[0];
        memmove(raw->queue, raw->queue + 1, --raw->queue_len * sizeof(raw->queue[0]));
    }

    return e;
}

// The first keysym of a plain-text key that is a named key here (Enter, Tab, arrows...), else VUI_KEY_NONE
static VuiKey named(const VuiVtRaw *raw, VuiVtEntry e)
{
    static const VuiKey cur[4] = { VUI_KEY_DOWN, VUI_KEY_LEFT, VUI_KEY_RIGHT, VUI_KEY_UP };
    static const VuiKey pad_off[10] = { VUI_KEY_INSERT, VUI_KEY_END, VUI_KEY_DOWN, VUI_KEY_PAGE_DOWN, VUI_KEY_LEFT,
                                        VUI_KEY_NONE, VUI_KEY_RIGHT, VUI_KEY_HOME, VUI_KEY_UP, VUI_KEY_PAGE_UP };

    switch (e.type) {
    case VT_FN:
        if (e.value < 12) return (VuiKey)(VUI_KEY_F1 + e.value);
        return e.value == 20 ? VUI_KEY_HOME : e.value == 21 ? VUI_KEY_INSERT : e.value == 22 ? VUI_KEY_DELETE
             : e.value == 23 ? VUI_KEY_END : e.value == 24 ? VUI_KEY_PAGE_UP : e.value == 25 ? VUI_KEY_PAGE_DOWN : VUI_KEY_NONE;
    case VT_CUR:  return e.value < 4 ? cur[e.value] : VUI_KEY_NONE;
    case VT_SPEC: return e.value == K_ENTER ? VUI_KEY_ENTER : VUI_KEY_NONE;
    case VT_PAD:
        if (e.value == 14) return VUI_KEY_ENTER;
        if (!raw->num && e.value < 10) return pad_off[e.value];
        return !raw->num && e.value == 16 ? VUI_KEY_DELETE : VUI_KEY_NONE;
    case VT_CHAR:
        return e.value == 0x1b ? VUI_KEY_ESCAPE : (e.value == 0x7f || e.value == 0x08) ? VUI_KEY_BACKSPACE
             : e.value == 0x09 ? VUI_KEY_TAB : e.value == 0x0d ? VUI_KEY_ENTER : VUI_KEY_NONE;
    default:      return VUI_KEY_NONE;
    }
}

// Keypad keys with NumLock on type their character
static uint32_t pad_char(const VuiVtRaw *raw, VuiVtEntry e)
{
    static const char ch[] = "0123456789+-*/\n,.";

    if (e.type != VT_PAD || e.value > 16 || e.value == 14) return 0;
    return (e.value >= 10 || raw->num) ? (uint32_t)(unsigned char)ch[e.value] : 0;
}

static uint32_t combine(const VuiVtRaw *raw, uint32_t d, uint32_t ch)
{
    for (unsigned int i = 0; i < raw->accents; i++) {
        if (raw->accent[i].diacr == d && raw->accent[i].base == ch) return raw->accent[i].result;
    }

    return 0;
}

static void press(VuiVtRaw *raw, unsigned int code, int was_down)
{
    VuiVtEntry e0 = raw->map[0][code];
    int shift = 0, altgr = 0, ctrl = 0, alt = 0;
    VuiModifiers mods;
    VuiScancode sc = vui_scancode_from_linux(code);
    VuiKey key;
    VuiEvent ev = { .type = VUI_EVENT_KEY };

    for (unsigned int k = 0; k < VUI_VT_KEYS; k++) {
        unsigned int g;

        if (!raw->down[k] || raw->map[0][k].type != VT_SHIFT) continue;
        g = raw->map[0][k].value;
        shift |= g == KG_SHIFT || g == KG_SHIFTL || g == KG_SHIFTR || g == KG_CAPSSHIFT;
        altgr |= g == KG_ALTGR;
        ctrl |= g == KG_CTRL || g == KG_CTRLL || g == KG_CTRLR;
        alt |= g == KG_ALT;
    }

    mods = (shift ? VUI_MOD_SHIFT : 0) | (ctrl ? VUI_MOD_CTRL : 0) | (alt ? VUI_MOD_ALT : 0);

    if (e0.type == VT_FN && e0.value < 12 && ctrl && alt) { // the kernel's VT switch, which raw mode turned off
        raw->switch_to = (int)e0.value + 1;
        return;
    }

    if (e0.type == VT_SPEC && (e0.value == K_CAPS || e0.value == K_NUM)) {
        if (!was_down) (e0.value == K_CAPS ? &raw->caps : &raw->num)[0] ^= 1;
        return;
    }

    key = named(raw, e0);
    if (key != VUI_KEY_NONE) {
        ev.key.key = key;
        ev.key.modifiers = mods;
        ev.key.scancode = sc;
        push(raw, ev);
        return;
    }

    {
        int idx = (shift ? 1 : 0) | (altgr ? 2 : 0);
        VuiVtEntry e = raw->map[idx ^ ((raw->caps && e0.type == VT_LETTER) ? 1 : 0)][code];
        VuiVtEntry plain = raw->map[altgr ? 2 : 0][code];
        uint32_t cp = pad_char(raw, e0);

        if (cp == 0 && (e.type == VT_CHAR || e.type == VT_LETTER)) cp = e.value;

        if (ctrl || alt) {
            // Ctrl/Alt: the key and its modifiers. The character is what the key types alone, Shift is the flag.
            uint32_t base = plain.type == VT_CHAR || plain.type == VT_LETTER ? plain.value : pad_char(raw, e0);

            if (base == 0) return;
            ev.key.key = VUI_KEY_CHAR;
            ev.key.character = base;
            ev.key.base = vui_scancode_base(sc) != 0 ? vui_scancode_base(sc) : base;
            ev.key.modifiers = mods;
            ev.key.scancode = sc;
            push(raw, ev);
            return;
        }

        if (e.type == VT_DEAD) {
            if (raw->dead != 0) push_text(raw, raw->dead);
            raw->dead = raw->dead == e.value ? 0 : e.value;
            return;
        }

        if (cp < 0x20 || cp == 0x7f) return;

        if (raw->dead != 0) { // the kernel's rules: combine, or a space / the same accent gives the accent itself,
            uint32_t d = raw->dead, r = combine(raw, d, cp); // else the accent and then the letter

            raw->dead = 0;
            if (r != 0) { push_text(raw, r); return; }
            push_text(raw, d);
            if (cp == ' ' || cp == d) return;
        }

        push_text(raw, cp);
    }
}

VuiEvent vui_vtraw_feed(VuiVtRaw *raw, uint8_t b)
{
    unsigned int code;
    int down;

    if (raw->pending == 0) {
        if (b == 0x00 || b == 0x80) { // keycodes above 127: this byte, (code >> 7) | 0x80, (code & 0x7f) | 0x80
            raw->pending = 1;
            raw->pending_down = b == 0x00;
            return vui_vtraw_pop(raw);
        }
        code = b & 0x7f;
        down = !(b & 0x80);
    } else if (raw->pending == 1) {
        raw->pending_code = (unsigned int)(b & 0x7f) << 7;
        raw->pending = 2;
        return vui_vtraw_pop(raw);
    } else {
        code = raw->pending_code | (b & 0x7f);
        down = raw->pending_down;
        raw->pending = 0;
    }

    if (code >= VUI_VT_KEYS) return vui_vtraw_pop(raw);

    if (down) {
        int was = raw->down[code];

        raw->down[code] = 1;
        press(raw, code, was);
    } else {
        raw->down[code] = 0;
    }

    return vui_vtraw_pop(raw);
}
