// GPM client structs (pure). `make test`.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gpm.h"

static int DY;
static void ev(unsigned char *b, int buttons, int mods, int x, int y, int type, int wdy)
{
    short s[4] = { 0, (short)DY, (short)x, (short)y };
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
    VuiEvent e, o[2];
    VuiGpmState st;
    unsigned short masks[4];
    int tail[2];

    vui_gpm_state_init(&st, 0);
    vui_gpm_hello(h, 1234, 3);
    memcpy(masks, h, 8); memcpy(tail, h + 8, 8);
    assert(masks[0] == 15 && masks[1] == 0 && masks[3] == 0xFFFF && tail[0] == 1234 && tail[1] == 3);

    ev(b, 4, 0, 10, 5, 4, 0);  assert((vui_gpm_event(b, 1, &st, o) > 0 && ((e = o[0]), 1)) && e.mouse.action == VUI_MOUSE_PRESS && e.mouse.button == VUI_MOUSE_LEFT && e.mouse.x == 9 && e.mouse.y == 4);
    ev(b, 0, 0, 10, 5, 8, 0);  assert(!(vui_gpm_event(b, 1, &st, o) > 0 && ((e = o[0]), 1))); // an UP with no button left is nothing we can name
    ev(b, 4, 0, 10, 5, 8, 0);  assert((vui_gpm_event(b, 1, &st, o) > 0 && ((e = o[0]), 1)) && e.mouse.action == VUI_MOUSE_RELEASE && e.mouse.button == VUI_MOUSE_LEFT);
    ev(b, 4, 0, 11, 5, 2, 0);  assert((vui_gpm_event(b, 1, &st, o) > 0 && ((e = o[0]), 1)) && e.mouse.action == VUI_MOUSE_MOVE && e.mouse.button == VUI_MOUSE_LEFT);
    ev(b, 0, 0, 11, 5, 1, 0);  assert(!(vui_gpm_event(b, 1, &st, o) > 0 && ((e = o[0]), 1)) && (vui_gpm_event(b, 2, &st, o) > 0 && ((e = o[0]), 1)) && e.mouse.button == VUI_MOUSE_NONE);
    ev(b, 1, 1 | 4, 3, 3, 4, 0); assert((vui_gpm_event(b, 1, &st, o) > 0 && ((e = o[0]), 1)) && e.mouse.button == VUI_MOUSE_RIGHT && e.mouse.modifiers == (VUI_MOD_SHIFT | VUI_MOD_CTRL));
    ev(b, 2, 8, 3, 3, 4, 0);   assert((vui_gpm_event(b, 1, &st, o) > 0 && ((e = o[0]), 1)) && e.mouse.button == VUI_MOUSE_MIDDLE && e.mouse.modifiers == VUI_MOD_ALT);
    ev(b, 0, 0, 3, 3, 1, 1);   assert((vui_gpm_event(b, 1, &st, o) > 0 && ((e = o[0]), 1)) && e.mouse.button == VUI_MOUSE_WHEEL_UP);
    ev(b, 0, 0, 3, 3, 1, -1);  assert((vui_gpm_event(b, 1, &st, o) > 0 && ((e = o[0]), 1)) && e.mouse.button == VUI_MOUSE_WHEEL_DOWN);
    ev(b, 4, 0, 10, 5, 4, 0);  assert(!(vui_gpm_event(b, 0, &st, o) > 0 && ((e = o[0]), 1))); // mouse off
    ev(b, 4, 0, 0, 0, 2, 0);   assert((vui_gpm_event(b, 1, &st, o) > 0 && ((e = o[0]), 1)) && e.mouse.x == 0 && e.mouse.y == 0); // a drag past the left/top edge stays at cell 0

    // middle button as a scroll lever (a TrackPoint): units of 10
    vui_gpm_state_init(&st, 10);
    DY = 0;  ev(b, 2, 0, 5, 5, 4, 0); assert(vui_gpm_event(b, 1, &st, o) == 0);                       // press held back
    DY = -4; ev(b, 2, 0, 5, 5, 2, 0); assert(vui_gpm_event(b, 1, &st, o) == 0);                       // not enough yet
    DY = -7; ev(b, 2, 0, 5, 5, 2, 0); assert(vui_gpm_event(b, 1, &st, o) == 1 && o[0].mouse.button == VUI_MOUSE_WHEEL_UP);
    DY = 5;  ev(b, 2, 0, 5, 5, 2, 0); assert(vui_gpm_event(b, 1, &st, o) == 0);                        // -1 + 5: below a step
    DY = 0;  ev(b, 2, 0, 5, 5, 8, 0); assert(vui_gpm_event(b, 1, &st, o) == 0);                       // it scrolled: no click
    vui_gpm_state_init(&st, 10);
    DY = 0;  ev(b, 2, 0, 5, 5, 4, 0); vui_gpm_event(b, 1, &st, o);
    ev(b, 2, 0, 5, 5, 8, 0); assert(vui_gpm_event(b, 1, &st, o) == 2 && o[0].mouse.action == VUI_MOUSE_PRESS && o[1].mouse.action == VUI_MOUSE_RELEASE && o[1].mouse.button == VUI_MOUSE_MIDDLE);
    DY = 20; ev(b, 0, 0, 5, 5, 1, 0); vui_gpm_state_init(&st, 10); st.mid = 1;
    assert(vui_gpm_event(b, 1, &st, o) == 1 && o[0].mouse.button == VUI_MOUSE_WHEEL_DOWN); // pushing down scrolls down
    // the pointer holds still during the lever and carries on from there
    vui_gpm_state_init(&st, 10); st.cols = 80; st.rows = 24;
    DY = 0;  ev(b, 2, 0, 10, 10, 4, 0); vui_gpm_event(b, 1, &st, o);                                   // middle down at (10,10)
    DY = -12; ev(b, 2, 0, 14, 12, 2, 0); assert(vui_gpm_event(b, 1, &st, o) == 1 && o[0].mouse.x == 9 && o[0].mouse.y == 9); // wheel, held at (10,10)
    DY = 0;  ev(b, 2, 0, 16, 12, 8, 0); vui_gpm_event(b, 1, &st, o);                                   // released after the daemon moved to (16,12)
    ev(b, 0, 0, 17, 12, 1, 0); assert(vui_gpm_event(b, 2, &st, o) == 1 && o[0].mouse.x == 10 && o[0].mouse.y == 9); // one step right of (10,10)
    ev(b, 0, 0, 80, 12, 1, 0); assert(vui_gpm_event(b, 2, &st, o) == 1 && o[0].mouse.x == 79);        // daemon at the right edge: ours too
    ev(b, 0, 0, 70, 12, 1, 0); assert(vui_gpm_event(b, 2, &st, o) == 1 && o[0].mouse.x == 69);        // offset gone
    puts("gpm: ok");
    return 0;
}
