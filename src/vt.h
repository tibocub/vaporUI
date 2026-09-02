#ifndef VAPORUI_VT_H
#define VAPORUI_VT_H

#include "../include/vaporui.h"

typedef enum {
    VUI_VT_NEED_MORE,
    VUI_VT_COMPLETE,
    VUI_VT_INVALID
} VuiVtResult;

typedef enum {
    VUI_VT_STATE_CSI,   // expecting '[' right after ESC
    VUI_VT_STATE_PARAM  // collecting an optional numeric param, then the final byte
} VuiVtState;

typedef struct {
    VuiVtState state;
    int param;
} VuiVtDecoder;

// Caller is expected to have already consumed the leading ESC (0x1B)
// before feeding bytes here -- this only decodes what follows it.
void vui_vt_reset(VuiVtDecoder *decoder);

VuiVtResult vui_vt_feed(
    VuiVtDecoder *decoder,
    uint8_t byte,
    VuiKey *key
);

#endif
