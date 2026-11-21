#include <string.h>

#include "gpm.h"

enum { MOVE = 1, DRAG = 2, DOWN = 4, UP = 8 };       // enum Gpm_Etype
enum { B_RIGHT = 1, B_MIDDLE = 2, B_LEFT = 4 };      // GPM_B_*

static int get16(const unsigned char *p) { return (short)(p[0] | (p[1] << 8)); }

// struct Gpm_Connect { ushort eventMask, defaultMask, minMod, maxMod; int pid; int vc; } in host byte order: the
// daemon and its client are on the same machine
void vui_gpm_hello(unsigned char out[VUI_GPM_HELLO_SIZE], int pid, int vc)
{
    unsigned short masks[4] = { MOVE | DRAG | DOWN | UP, 0, 0, 0xFFFF };
    int v[2] = { pid, vc };

    memcpy(out, masks, sizeof(masks));
    memcpy(out + 8, v, sizeof(v));
}

// struct Gpm_Event { uchar buttons, modifiers; ushort vc; short dx, dy, x, y; enum type; int clicks; enum margin;
// short wdx, wdy; }
int vui_gpm_event(const unsigned char in[VUI_GPM_EVENT_SIZE], int level, VuiEvent *out)
{
    unsigned buttons = in[0], mods = in[1], type;
    int x = get16(in + 8), y = get16(in + 10), wdy = get16(in + 26);
    VuiMouseEvent *m = &out->mouse;

    memcpy(&type, in + 12, sizeof(type)); // host byte order, like the rest of the struct

    if (level <= 0) return 0;

    // The daemon clamps to 1..cols, but a drag or a release may go one cell past the edge (0 or cols + 1, with the
    // margin flag set). Dropping those froze a selection at the left edge: keep them at the nearest cell.
    if (x < 1) x = 1;
    if (y < 1) y = 1;

    *out = (VuiEvent){ .type = VUI_EVENT_MOUSE };
    m->x = (unsigned)(x - 1);
    m->y = (unsigned)(y - 1);
    m->modifiers = (VuiModifiers)(((mods & 1) ? VUI_MOD_SHIFT : 0) | ((mods & 4) ? VUI_MOD_CTRL : 0) | ((mods & 8) ? VUI_MOD_ALT : 0));
    m->button = (buttons & B_LEFT) ? VUI_MOUSE_LEFT : (buttons & B_MIDDLE) ? VUI_MOUSE_MIDDLE : (buttons & B_RIGHT) ? VUI_MOUSE_RIGHT : VUI_MOUSE_NONE;

    if (wdy != 0) { // wheel: not a button; shortcut: the sign (positive = up) is the daemon's convention as I read it, not verified on hardware
        m->button = wdy > 0 ? VUI_MOUSE_WHEEL_UP : VUI_MOUSE_WHEEL_DOWN;
        m->action = VUI_MOUSE_PRESS;
        return 1;
    }

    if (type & DOWN) { m->action = VUI_MOUSE_PRESS; return m->button != VUI_MOUSE_NONE; }
    if (type & UP)   { m->action = VUI_MOUSE_RELEASE; return m->button != VUI_MOUSE_NONE; }
    if (type & (MOVE | DRAG)) {
        m->action = VUI_MOUSE_MOVE;
        return m->button != VUI_MOUSE_NONE || level >= 2;
    }

    return 0;
}
