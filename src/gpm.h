#ifndef VAPORUI_GPM_H
#define VAPORUI_GPM_H

#include "../include/vaporui.h"

// The Linux console has no mouse protocol of its own: the gpm daemon owns the mouse and talks to clients over the
// unix socket /dev/gpmctl. This file is the pure part (bytes <-> structs, no I/O): the backend connects, sends
// vui_gpm_hello(), and feeds each 28-byte event to vui_gpm_event(). Layouts are those of libgpm's Gpm_Connect and
// Gpm_Event on Linux (gpm.h); there is no libgpm dependency.

#define VUI_GPM_HELLO_SIZE 16
#define VUI_GPM_EVENT_SIZE 28

// The Gpm_Connect to send: we take every mouse event (move, drag, down, up, any modifiers). The daemon keeps handling
// plain moves itself (defaultMask = move), which is what draws its pointer (only root may draw one from a client), but
// presses and drags are ours: no gpm text selection while the app has the mouse; it comes back when the socket closes.
void vui_gpm_hello(unsigned char out[VUI_GPM_HELLO_SIZE], int pid, int vc);

// One Gpm_Event -> a mouse event. level is vui_set_mouse's (1 clicks and drags, 2 also plain motion). Returns 1 when
// there is something to report. x, y come 1-based from the daemon and leave 0-based.
int vui_gpm_event(const unsigned char in[VUI_GPM_EVENT_SIZE], int level, VuiEvent *out);

#endif
