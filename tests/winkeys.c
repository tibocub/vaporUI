// Windows record translation (pure), run on any platform. `make test`.
#include <assert.h>
#include <stdio.h>
#include "winkeys.h"
#include "scancode.h"

#define VK_A 0x41
static VuiEvent key(unsigned vk, unsigned scan, unsigned uc, unsigned unshifted, unsigned st, int *ok)
{
    VuiEvent e = {0};
    *ok = vui_winkey_translate(vk, scan, uc, unshifted, st, &e);
    return e;
}

int main(void)
{
    int ok;
    VuiEvent e;

    // plain typing is text; Shift is already in the character
    e = key(VK_A, 0x1E, 'a', 'a', 0, &ok); assert(ok && e.type == VUI_EVENT_TEXT && e.text.codepoint == 'a');
    e = key(VK_A, 0x1E, 'A', 'a', VUI_WIN_SHIFT, &ok); assert(ok && e.type == VUI_EVENT_TEXT && e.text.codepoint == 'A');
    // AZERTY: the é key (scan 3), plain, and the & key
    e = key(0x32, 0x03, 0xE9, 0xE9, 0, &ok); assert(ok && e.type == VUI_EVENT_TEXT && e.text.codepoint == 0xE9);
    // Alt+é: key é, base '2', physical 2; Alt+Shift+é the same with Shift
    e = key(0x32, 0x03, 0xE9, 0xE9, VUI_WIN_LEFT_ALT, &ok);
    assert(ok && e.key.key == VUI_KEY_CHAR && e.key.character == 0xE9 && e.key.base == '2' && e.key.scancode == VUI_SC_2 && e.key.modifiers == VUI_MOD_ALT);
    e = key(0x32, 0x03, '2', 0xE9, VUI_WIN_LEFT_ALT | VUI_WIN_SHIFT, &ok);
    assert(ok && e.key.character == 0xE9 && e.key.base == '2' && e.key.modifiers == (VUI_MOD_ALT | VUI_MOD_SHIFT));
    // Ctrl+é: Windows sends no usable character, we still know the key
    e = key(0x32, 0x03, 0, 0xE9, VUI_WIN_LEFT_CTRL, &ok);
    assert(ok && e.key.character == 0xE9 && e.key.base == '2' && e.key.modifiers == VUI_MOD_CTRL);
    // Ctrl+A arrives as a control code
    e = key(VK_A, 0x1E, 1, 'a', VUI_WIN_LEFT_CTRL, &ok);
    assert(ok && e.key.character == 'a' && e.key.base == 'a' && e.key.modifiers == VUI_MOD_CTRL);
    // AZERTY: Ctrl+A is the key labelled A, which is the Q position
    e = key(VK_A, 0x10, 1, 'a', VUI_WIN_LEFT_CTRL, &ok);
    assert(ok && e.key.character == 'a' && e.key.base == 'q' && e.key.scancode == VUI_SC_Q);
    // AltGr (Right Alt + Left Ctrl) that typed a character is text: AltGr+é = ~
    e = key(0x32, 0x03, '~', 0xE9, VUI_WIN_RIGHT_ALT | VUI_WIN_LEFT_CTRL, &ok);
    assert(ok && e.type == VUI_EVENT_TEXT && e.text.codepoint == '~');
    // named keys, modifiers, scancodes
    e = key(0x25, 0x4B, 0, 0, VUI_WIN_ENHANCED | VUI_WIN_LEFT_CTRL, &ok);
    assert(ok && e.key.key == VUI_KEY_LEFT && e.key.modifiers == VUI_MOD_CTRL && e.key.scancode == VUI_SC_LEFT);
    e = key(0x09, 0x0F, 9, 0, VUI_WIN_SHIFT, &ok); assert(ok && e.key.key == VUI_KEY_TAB && e.key.modifiers == VUI_MOD_SHIFT);
    e = key(0x71, 0x3C, 0, 0, VUI_WIN_LEFT_ALT, &ok); assert(ok && e.key.key == VUI_KEY_F2 && e.key.modifiers == VUI_MOD_ALT);
    e = key(0x0D, 0x1C, 13, 0, 0, &ok); assert(ok && e.key.key == VUI_KEY_ENTER);
    // modifiers alone, locks and dead keys are nothing
    key(0x11, 0x1D, 0, 0, VUI_WIN_LEFT_CTRL, &ok); assert(!ok);
    key(0x10, 0x2A, 0, 0, VUI_WIN_SHIFT, &ok); assert(!ok);
    key(0x14, 0x3A, 0, 0, 0, &ok); assert(!ok);
    key(0xDD, 0x1A, 0, 0, 0, &ok); assert(!ok); // dead key (^ on AZERTY): nothing until its partner
    // Ctrl+Space, Alt+Space
    e = key(0x20, 0x39, ' ', ' ', VUI_WIN_LEFT_CTRL, &ok); assert(ok && e.key.character == ' ' && e.key.base == ' ' && e.key.modifiers == VUI_MOD_CTRL);

    // mouse
    e = (VuiEvent){0};
    assert(vui_winmouse_translate(1, 0, 0, 5, 6, 0, 1, &e) && e.mouse.button == VUI_MOUSE_LEFT && e.mouse.action == VUI_MOUSE_PRESS && e.mouse.x == 5 && e.mouse.y == 6);
    assert(vui_winmouse_translate(0, 1, 0, 5, 6, 0, 1, &e) && e.mouse.button == VUI_MOUSE_LEFT && e.mouse.action == VUI_MOUSE_RELEASE);
    assert(vui_winmouse_translate(1, 1, 1, 7, 6, 0, 1, &e) && e.mouse.action == VUI_MOUSE_MOVE && e.mouse.button == VUI_MOUSE_LEFT); // drag
    assert(!vui_winmouse_translate(0, 0, 1, 7, 6, 0, 1, &e)); // hover at level 1: not reported
    assert(vui_winmouse_translate(0, 0, 1, 7, 6, 0, 2, &e) && e.mouse.action == VUI_MOUSE_MOVE && e.mouse.button == VUI_MOUSE_NONE);
    assert(vui_winmouse_translate(0x00780000u, 0, 4, 1, 1, 0, 1, &e) && e.mouse.button == VUI_MOUSE_WHEEL_UP);
    assert(vui_winmouse_translate(0xFF880000u, 0, 4, 1, 1, VUI_WIN_SHIFT, 1, &e) && e.mouse.button == VUI_MOUSE_WHEEL_DOWN && e.mouse.modifiers == VUI_MOD_SHIFT);
    assert(vui_winmouse_translate(2, 0, 0, 1, 1, 0, 1, &e) && e.mouse.button == VUI_MOUSE_RIGHT);
    assert(!vui_winmouse_translate(1, 0, 0, 1, 1, 0, 0, &e)); // mouse off

    puts("winkeys: ok");
    return 0;
}
