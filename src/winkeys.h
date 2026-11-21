#ifndef VAPORUI_WINKEYS_H
#define VAPORUI_WINKEYS_H

#include "../include/vaporui.h"

// Windows console records -> vaporUI events. Pure (no windows.h): the Windows backend fills in the numbers, so the
// logic is tested on any platform (tests/winkeys.c). dwControlKeyState bits are the Win32 ones.

#define VUI_WIN_RIGHT_ALT  0x0001u
#define VUI_WIN_LEFT_ALT   0x0002u
#define VUI_WIN_RIGHT_CTRL 0x0004u
#define VUI_WIN_LEFT_CTRL  0x0008u
#define VUI_WIN_SHIFT      0x0010u
#define VUI_WIN_ENHANCED   0x0100u

// A KEY_EVENT_RECORD with bKeyDown set. `unshifted` is what the key types on the current layout with no modifier
// held (ToUnicodeEx with an empty key state; 0 if unknown). Returns 1 and fills *out, or 0 when the record is no key
// press for the app: a modifier or lock key alone, a dead key.
//  - AltGr (Right Alt + Left Ctrl, which Windows reports for it) that produced a character is plain text
//  - typing with no Ctrl/Alt is a TEXT event; with Ctrl or Alt it is a KEY: the character the key types without
//    Shift, plus the Shift flag, plus the physical key (scancode) and its US position (base)
int vui_winkey_translate(unsigned vk, unsigned scan, uint32_t uc, uint32_t unshifted, unsigned ctrl_state, VuiEvent *out);

// For vui_set_key_release(): the event for a key going UP (down == 0), or for a modifier / lock key going down (those
// aren't keys for vui_winkey_translate). Returns 0 for a press of anything else. key is VUI_KEY_NONE for modifiers and
// locks, else the named key, or VUI_KEY_CHAR with `unshifted` (or the US character of the scancode); .release is set.
int vui_winkey_state(unsigned vk, unsigned scan, int down, uint32_t unshifted, unsigned ctrl_state, VuiEvent *out);

// A MOUSE_EVENT_RECORD. x, y are cells relative to the visible window. `level` is vui_set_mouse's (0 off, 1 clicks and
// drags, 2 also plain motion). Returns 1 and fills *out when there is something to report.
// buttons: dwButtonState (low word: 1 left, 2 right, 4 middle; high word: wheel delta), flags: dwEventFlags
int vui_winmouse_translate(unsigned buttons, unsigned prev_buttons, unsigned flags, unsigned x, unsigned y, unsigned ctrl_state, int level, VuiEvent *out);

#endif
