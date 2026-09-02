#ifndef VAPORUI_UTF8_H
#define VAPORUI_UTF8_H

#include <stdint.h>

typedef enum {
    VUI_UTF8_NEED_MORE,
    VUI_UTF8_COMPLETE,
    VUI_UTF8_INVALID
} VuiUtf8Result;

typedef struct {
    uint32_t codepoint;
    uint8_t expected_continuations;
} VuiUtf8Decoder;

void vui_utf8_reset(VuiUtf8Decoder *decoder);

VuiUtf8Result vui_utf8_feed(
    VuiUtf8Decoder *decoder,
    uint8_t byte,
    uint32_t *codepoint
);

// Encodes a codepoint back into UTF-8 bytes. buf must have room for
// at least 4 bytes. Returns the number of bytes written (1-4), or 0
// if the codepoint is out of Unicode range.
int vui_utf8_encode(uint32_t codepoint, uint8_t *buf);

#endif
