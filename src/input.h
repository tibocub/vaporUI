#ifndef VAPORUI_INPUT_H
#define VAPORUI_INPUT_H

#include <stdint.h>

#include "../include/vaporui.h"
#include "utf8.h"
#include "vt.h"

typedef struct {
    int in_escape;
    int in_paste; // between PASTE_START and PASTE_END
    int alt; // ESC prefixed the char being decoded
    int shift_state; // Shift held when that ESC-prefixed char arrived (from the shift probe): 0, 1, or -1 unknown
    VuiVtDecoder vt;
    VuiUtf8Decoder utf8;
} VuiInputDecoder;

void vui_input_reset(VuiInputDecoder *decoder);

// Some consoles lose modifiers: the Linux console, French keymap, sends Alt+e-acute and Alt+Shift+e-acute both as
// ESC 2, and Ctrl+& as a plain '&'. A backend that can ask the console which modifiers are down right now installs
// a probe: it returns the VuiModifiers held (VUI_MOD_SHIFT | VUI_MOD_ALT | VUI_MOD_CTRL), or -1 if it cannot tell. It
// is asked for ESC-prefixed characters, for Ctrl+letter bytes and for plain typed characters (to catch Ctrl held).
typedef int (*VuiShiftProbe)(void);
void vui_input_set_shift_probe(VuiShiftProbe probe);

// Feed one raw byte from the terminal. Returns VUI_EVENT_NONE when the
// byte was consumed but didn't complete anything yet (mid UTF-8
// sequence or mid escape sequence) -- callers should keep polling.
VuiEvent vui_input_feed(VuiInputDecoder *decoder, uint8_t byte);

// True once a lone ESC (0x1B) has been fed and we're waiting to see
// if a '[' follows. Platform layers use this to shorten their read
// timeout, since a standalone Escape keypress never sends more bytes.
int vui_input_pending_escape(const VuiInputDecoder *decoder);

// Call when a read times out while vui_input_pending_escape() was
// true: gives up on the sequence and reports a standalone Escape.
VuiEvent vui_input_timeout(VuiInputDecoder *decoder);

// Set by the backend: the keyboard layout name the OS reports ("fr", "us", "fr(oss)"...), or NULL when it can't tell.
// vui_set_layout(NULL) uses it when $VUI_LAYOUT is not set.
extern const char *(*vui_layout_hook)(void);

#endif
