#include "utf8.h"

void vui_utf8_reset(VuiUtf8Decoder *decoder)
{
    decoder->codepoint = 0;
    decoder->expected_continuations = 0;
}

VuiUtf8Result vui_utf8_feed(
    VuiUtf8Decoder *decoder,
    uint8_t byte,
    uint32_t *codepoint)
{
    if (decoder->expected_continuations > 0) {

        // A continuation byte must be 10xxxxxx
        // 0xC0 = 11000000, 0x80 = 10000000
        if ((byte & 0xC0) != 0x80) {
            vui_utf8_reset(decoder);
            return VUI_UTF8_INVALID;
        }

        // Keeping the six bits payload
        // 0x3F = 00111111
        decoder->codepoint <<= 6;
        decoder->codepoint |= byte & 0x3F;

        decoder->expected_continuations--;

        if (decoder->expected_continuations > 0) {
            return VUI_UTF8_NEED_MORE;
        }

        *codepoint = decoder->codepoint;
        vui_utf8_reset(decoder);

        return VUI_UTF8_COMPLETE;
    }

    // ASCII must be 0xxxxxxx
    if ((byte & 0x80) == 0) {
        *codepoint = byte;
        return VUI_UTF8_COMPLETE;
    }

    // 2-byte sequence:
    // 110xxxxx 10xxxxxx
    if ((byte & 0xE0) == 0xC0) {
        decoder->codepoint = byte & 0x1F;
        decoder->expected_continuations = 1;

        return VUI_UTF8_NEED_MORE;
    }

    // 3-byte sequence:
    // 1110xxxx 10xxxxxx 10xxxxxx
    if ((byte & 0xF0) == 0xE0) {
        decoder->codepoint = byte & 0x0F;
        decoder->expected_continuations = 2;

        return VUI_UTF8_NEED_MORE;
    }

    // 4-byte sequence:
    // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    if ((byte & 0xF8) == 0xF0) {
        decoder->codepoint = byte & 0x07;
        decoder->expected_continuations = 3;

        return VUI_UTF8_NEED_MORE;
    }

    return VUI_UTF8_INVALID;
}
