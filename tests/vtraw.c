// Linux console raw keys, on a synthetic French keymap (pure, any platform). `make test`.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "vtraw.h"
#include "scancode.h"

static VuiVtRaw raw;
static VuiEvent got[8];
static int ngot;

#define KV(t, v) (((t) << 8) | (v))
#define UNI(c) ((unsigned)(c) ^ 0xF000u) // what KDGKBENT returns for a Unicode character

static void set(int code, unsigned plain, unsigned shift, unsigned altgr)
{
    raw.map[0][code] = vui_vtraw_entry(plain);
    raw.map[1][code] = vui_vtraw_entry(shift);
    raw.map[2][code] = vui_vtraw_entry(altgr);
    raw.map[3][code] = vui_vtraw_entry(shift);
}

static void feed(int code, int down)
{
    VuiEvent e;
    uint8_t b[3];
    int n = 0;

    if (code < 128) b[n++] = (uint8_t)(code | (down ? 0 : 0x80));
    else { b[n++] = down ? 0 : 0x80; b[n++] = (uint8_t)((code >> 7) | 0x80); b[n++] = (uint8_t)(code | 0x80); }

    if (down) ngot = 0;
    for (int i = 0; i < n; i++) {
        e = vui_vtraw_feed(&raw, b[i]);
        while (e.type != VUI_EVENT_NONE) { got[ngot++] = e; e = vui_vtraw_pop(&raw); }
    }
}

static void tap(int code) { feed(code, 1); feed(code, 0); }

int main(void)
{
    memset(&raw, 0, sizeof raw);
    set(1, KV(0, 0x1b), KV(0, 0x1b), 0);                  // Esc
    set(2, KV(0, '&'), KV(0, '1'), 0);
    set(3, UNI(0xE9), KV(0, '2'), KV(0, '~'));            // é  2  ~
    set(14, KV(0, 0x7f), KV(0, 0x7f), 0);
    set(15, KV(0, 9), KV(0, 9), 0);
    set(16, KV(11, 'a'), KV(11, 'A'), 0);                 // AZERTY a, on the Q key
    set(26, KV(4, 2), KV(4, 4), 0);                       // dead circumflex, dead diaeresis
    set(28, KV(2, 1), KV(2, 1), 0);                       // Enter
    set(29, KV(7, 2), KV(7, 2), 0);                       // Ctrl
    set(30, KV(11, 'q'), KV(11, 'Q'), 0);
    set(42, KV(7, 0), KV(7, 0), 0);                       // Shift
    set(56, KV(7, 3), KV(7, 3), 0);                       // Alt
    set(57, KV(0, ' '), KV(0, ' '), 0);
    set(58, KV(2, 7), KV(2, 7), 0);                       // CapsLock
    set(59, KV(1, 0), KV(1, 12), 0);                      // F1
    set(60, KV(1, 1), KV(1, 13), 0);                      // F2
    set(18, KV(11, 'e'), KV(11, 'E'), 0);
    set(79, KV(3, 1), KV(3, 1), 0);                       // keypad 1
    set(100, KV(7, 1), KV(7, 1), 0);                      // AltGr
    set(103, KV(6, 3), KV(6, 3), 0);                      // Up
    set(111, KV(1, 22), KV(1, 22), 0);                    // Delete
    set(200, KV(0, 'z'), KV(0, 'Z'), 0);                  // a key above 127
    raw.accents = 1;
    raw.accent[0] = (VuiVtAccent){ '^', 'e', 0xEA };
    vui_vtraw_init(&raw, 0);

    // plain typing is text; Shift and CapsLock change it; letters
    tap(30); assert(ngot == 1 && got[0].type == VUI_EVENT_TEXT && got[0].text.codepoint == 'q');
    tap(3);  assert(ngot == 1 && got[0].text.codepoint == 0xE9);
    feed(42, 1); tap(3); assert(got[0].text.codepoint == '2'); tap(30); assert(got[0].text.codepoint == 'Q'); feed(42, 0);
    tap(58); tap(30); assert(got[0].text.codepoint == 'Q'); tap(2); assert(got[0].text.codepoint == '&'); // caps: letters only
    feed(42, 1); tap(30); assert(got[0].text.codepoint == 'q'); feed(42, 0); tap(58);
    feed(100, 1); tap(3); assert(got[0].type == VUI_EVENT_TEXT && got[0].text.codepoint == '~'); feed(100, 0);

    // Alt+é, Alt+Shift+é, Ctrl+é, Ctrl+&: the key, its modifiers, the physical key
    feed(56, 1); tap(3);
    assert(ngot == 1 && got[0].type == VUI_EVENT_KEY && got[0].key.key == VUI_KEY_CHAR && got[0].key.character == 0xE9
           && got[0].key.base == '2' && got[0].key.scancode == VUI_SC_2 && got[0].key.modifiers == VUI_MOD_ALT);
    feed(42, 1); tap(3);
    assert(got[0].key.character == 0xE9 && got[0].key.modifiers == (VUI_MOD_ALT | VUI_MOD_SHIFT)); feed(42, 0);
    tap(2); assert(got[0].key.character == '&' && got[0].key.base == '1'); feed(56, 0);
    feed(29, 1); tap(3); assert(got[0].key.character == 0xE9 && got[0].key.base == '2' && got[0].key.modifiers == VUI_MOD_CTRL);
    tap(2); assert(got[0].key.character == '&' && got[0].key.base == '1');
    tap(16); assert(got[0].key.character == 'a' && got[0].key.base == 'q' && got[0].key.scancode == VUI_SC_Q); // AZERTY Ctrl+A
    feed(29, 0);

    // named keys
    tap(28); assert(got[0].key.key == VUI_KEY_ENTER);
    tap(15); assert(got[0].key.key == VUI_KEY_TAB);
    tap(1);  assert(got[0].key.key == VUI_KEY_ESCAPE);
    tap(14); assert(got[0].key.key == VUI_KEY_BACKSPACE);
    tap(103); assert(got[0].key.key == VUI_KEY_UP && got[0].key.scancode == VUI_SC_UP);
    tap(111); assert(got[0].key.key == VUI_KEY_DELETE);
    feed(56, 1); tap(60); assert(got[0].key.key == VUI_KEY_F2 && got[0].key.modifiers == VUI_MOD_ALT); feed(56, 0);
    feed(42, 1); tap(60); assert(got[0].key.key == VUI_KEY_F2 && got[0].key.modifiers == VUI_MOD_SHIFT); feed(42, 0); // not F14
    feed(29, 1); feed(56, 1); assert(raw.switch_to == 0); tap(59); assert(ngot == 0 && raw.switch_to == 1); raw.switch_to = 0;
    feed(56, 0); feed(29, 0);

    // keypad follows NumLock
    tap(79); assert(got[0].key.key == VUI_KEY_END);
    raw.num = 1; tap(79); assert(got[0].type == VUI_EVENT_TEXT && got[0].text.codepoint == '1'); raw.num = 0;

    // dead keys: ^ e = ê, ^ space = ^, ^ q = ^q, ^ ^ = ^
    tap(26); assert(ngot == 0); tap(18); assert(ngot == 1 && got[0].text.codepoint == 0xEA);
    tap(26); tap(57); assert(ngot == 1 && got[0].text.codepoint == '^');
    tap(26); tap(30); assert(ngot == 2 && got[0].text.codepoint == '^' && got[1].text.codepoint == 'q');
    tap(26); tap(26); assert(ngot == 1 && got[0].text.codepoint == '^');

    // keycodes above 127 come as three bytes
    tap(200); assert(ngot == 1 && got[0].text.codepoint == 'z');
    // key repeat is a press without a release
    feed(30, 1); feed(30, 1); assert(ngot == 1); feed(30, 0);
    // a Ctrl+Alt+F-key forgets nothing by itself; release_all does
    feed(29, 1); vui_vtraw_release_all(&raw); tap(30); assert(got[0].type == VUI_EVENT_TEXT);

    // releases and bare modifiers only when asked: Shift down/up, then a key held (repeat) and let go
    vui_set_key_release(1);
    feed(42, 1); assert(ngot == 1 && got[0].key.key == VUI_KEY_NONE && got[0].key.scancode == VUI_SC_LSHIFT && got[0].key.modifiers == VUI_MOD_SHIFT && !got[0].key.release);
    feed(42, 0); assert(ngot == 2 && got[1].key.key == VUI_KEY_NONE && got[1].key.release && got[1].key.modifiers == 0);
    feed(30, 1); assert(ngot == 1 && !got[0].key.repeat);
    feed(30, 1); assert(ngot == 1 && got[0].type == VUI_EVENT_TEXT); // plain typing stays text
    feed(29, 1); feed(30, 1); assert(got[ngot - 1].key.repeat == 0 || got[ngot - 1].key.key == VUI_KEY_CHAR);
    feed(30, 1); assert(got[ngot - 1].key.key == VUI_KEY_CHAR && got[ngot - 1].key.repeat); // Ctrl+q held: a repeat
    feed(30, 0); assert(got[ngot - 1].key.release && got[ngot - 1].key.key == VUI_KEY_CHAR && got[ngot - 1].key.character == 'q' && got[ngot - 1].key.modifiers == VUI_MOD_CTRL);
    feed(29, 0); assert(got[ngot - 1].key.release && got[ngot - 1].key.key == VUI_KEY_NONE && got[ngot - 1].key.scancode == VUI_SC_LCTRL && got[ngot - 1].key.modifiers == 0);
    vui_set_key_release(0);
    feed(30, 1); feed(30, 0); assert(ngot == 1); // off again: no release event

    puts("vtraw: ok");
    return 0;
}
