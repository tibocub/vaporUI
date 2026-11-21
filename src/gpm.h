#ifndef VAPORUI_GPM_H
#define VAPORUI_GPM_H

#include "../include/vaporui.h"

// The Linux console has no mouse protocol of its own: the gpm daemon owns the mouse and talks to clients over the
// unix socket /dev/gpmctl. This file is the pure part (bytes <-> structs, no I/O): the backend connects, sends
// vui_gpm_hello(), and feeds each 28-byte event to vui_gpm_event(). Layouts are those of libgpm's Gpm_Connect and
// Gpm_Event on Linux (gpm.h); there is no libgpm dependency.

#define VUI_GPM_HELLO_SIZE 16
#define VUI_GPM_EVENT_SIZE 28

// The Gpm_Connect to send: we take every mouse event (move, drag, down, up, any modifiers) and leave nothing to the
// daemon's own pointer and selection (defaultMask 0), so a program that asked for the mouse has it, and loses it when
// the socket closes. The daemon then draws no pointer: vaporUI draws its own (vui_soft_pointer).
void vui_gpm_hello(unsigned char out[VUI_GPM_HELLO_SIZE], int pid, int vc);

// Wheel emulation for pointing devices that have no wheel (a ThinkPad TrackPoint, most touchpads under gpm): while the
// middle button is held, vertical motion scrolls, one wheel step per `units` of movement (up = wheel up). A middle
// click that did not scroll is still reported as a click, when the button goes up.
typedef struct { int units, mid, scrolled, acc; } VuiGpmState;
void vui_gpm_state_init(VuiGpmState *st, int units); // units 0: no emulation

// One Gpm_Event -> up to two mouse events (out[0], out[1]); returns how many. level is vui_set_mouse's (1 clicks and
// drags, 2 also plain motion). x, y come 1-based from the daemon and leave 0-based.
int vui_gpm_event(const unsigned char in[VUI_GPM_EVENT_SIZE], int level, VuiGpmState *st, VuiEvent out[2]);

#endif
