// GPM client structs (pure). `make test`.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gpm.h"

static void ev(unsigned char *b, int buttons, int mods, int x, int y, int type, int wdy)
{
    short s[4] = { 0, 0, (short)x, (short)y };
    short w[2] = { 0, (short)wdy };

    memset(b, 0, VUI_GPM_EVENT_SIZE);
    b[0] = (unsigned char)buttons;
    b[1] = (unsigned char)mods;
    memcpy(b + 4, s, sizeof(s));
    memcpy(b + 12, &type, sizeof(type));
    memcpy(b + 24, w, sizeof(w));
}

int main(void)
{
    unsigned char b[VUI_GPM_EVENT_SIZE], h[VUI_GPM_HELLO_SIZE];
    VuiEvent e;
    unsigned short masks[4];
    int tail[2];

    vui_gpm_hello(h, 1234, 3);
    memcpy(masks, h, 8); memcpy(tail, h + 8, 8);
    assert(masks[0] == 15 && masks[1] == 1 && masks[3] == 0xFFFF && tail[0] == 1234 && tail[1] == 3);

    ev(b, 4, 0, 10, 5, 4, 0);  assert(vui_gpm_event(b, 1, &e) && e.mouse.action == VUI_MOUSE_PRESS && e.mouse.button == VUI_MOUSE_LEFT && e.mouse.x == 9 && e.mouse.y == 4);
    ev(b, 0, 0, 10, 5, 8, 0);  assert(!vui_gpm_event(b, 1, &e)); // an UP with no button left is nothing we can name
    ev(b, 4, 0, 10, 5, 8, 0);  assert(vui_gpm_event(b, 1, &e) && e.mouse.action == VUI_MOUSE_RELEASE && e.mouse.button == VUI_MOUSE_LEFT);
    ev(b, 4, 0, 11, 5, 2, 0);  assert(vui_gpm_event(b, 1, &e) && e.mouse.action == VUI_MOUSE_MOVE && e.mouse.button == VUI_MOUSE_LEFT);
    ev(b, 0, 0, 11, 5, 1, 0);  assert(!vui_gpm_event(b, 1, &e) && vui_gpm_event(b, 2, &e) && e.mouse.button == VUI_MOUSE_NONE);
    ev(b, 1, 1 | 4, 3, 3, 4, 0); assert(vui_gpm_event(b, 1, &e) && e.mouse.button == VUI_MOUSE_RIGHT && e.mouse.modifiers == (VUI_MOD_SHIFT | VUI_MOD_CTRL));
    ev(b, 2, 8, 3, 3, 4, 0);   assert(vui_gpm_event(b, 1, &e) && e.mouse.button == VUI_MOUSE_MIDDLE && e.mouse.modifiers == VUI_MOD_ALT);
    ev(b, 0, 0, 3, 3, 1, 1);   assert(vui_gpm_event(b, 1, &e) && e.mouse.button == VUI_MOUSE_WHEEL_UP);
    ev(b, 0, 0, 3, 3, 1, -1);  assert(vui_gpm_event(b, 1, &e) && e.mouse.button == VUI_MOUSE_WHEEL_DOWN);
    ev(b, 4, 0, 10, 5, 4, 0);  assert(!vui_gpm_event(b, 0, &e)); // mouse off
    ev(b, 4, 0, 0, 0, 4, 0);   assert(!vui_gpm_event(b, 1, &e)); // 1-based: 0 is not a cell

    puts("gpm: ok");
    return 0;
}
