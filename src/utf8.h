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

#endif
