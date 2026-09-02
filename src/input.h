#ifndef VAPORUI_INPUT_H
#define VAPORUI_INPUT_H

#include <stdint.h>

#include "../include/vaporui.h"
#include "utf8.h"
#include "vt.h"

typedef struct {
    int in_escape;
    VuiVtDecoder vt;
    VuiUtf8Decoder utf8;
} VuiInputDecoder;

void vui_input_reset(VuiInputDecoder *decoder);

// Feed one raw byte from the terminal. Returns VUI_EVENT_NONE when the
// byte was consumed but didn't complete anything yet (mid UTF-8
// sequence or mid escape sequence) -- callers should keep polling.
VuiEvent vui_input_feed(VuiInputDecoder *decoder, uint8_t byte);

#endif
