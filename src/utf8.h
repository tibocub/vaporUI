#ifndef VAPORUI_UTF8_H
#define VAPORUI_UTF8_H

#include <stddef.h>
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

// Decodes a NUL-terminated string into at most cap codepoints; returns the count.
unsigned int vui_utf8_to_cps(const char *str, uint32_t *out, unsigned int cap);

// Encodes n codepoints as NUL-terminated UTF-8, never splitting a
// character when size is too small; returns the bytes written (without the NUL).
size_t vui_cps_to_utf8(const uint32_t *cps, unsigned int n, char *out, size_t size);

#endif
