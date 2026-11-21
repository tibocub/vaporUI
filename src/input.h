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

// Some consoles send Alt+key as ESC <char> whether or not Shift is held (the Linux console, French keymap: Alt+e-acute
// and Alt+Shift+e-acute are both ESC 2). A backend that can ask the console whether Shift is down right now installs
// a probe (returns 1, 0, or -1 if it cannot tell); it is consulted for ESC-prefixed characters only.
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

#endif
