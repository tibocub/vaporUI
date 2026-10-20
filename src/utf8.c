#include <string.h>
#include "../include/vaporui.h"
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

int vui_utf8_encode(uint32_t codepoint, uint8_t *buf)
{
    if (codepoint <= 0x7F) {
        buf[0] = (uint8_t)codepoint;
        return 1;
    }

    if (codepoint <= 0x7FF) {
        buf[0] = 0xC0 | (codepoint >> 6);
        buf[1] = 0x80 | (codepoint & 0x3F);
        return 2;
    }

    if (codepoint <= 0xFFFF) {
        buf[0] = 0xE0 | (codepoint >> 12);
        buf[1] = 0x80 | ((codepoint >> 6) & 0x3F);
        buf[2] = 0x80 | (codepoint & 0x3F);
        return 3;
    }

    if (codepoint <= 0x10FFFF) {
        buf[0] = 0xF0 | (codepoint >> 18);
        buf[1] = 0x80 | ((codepoint >> 12) & 0x3F);
        buf[2] = 0x80 | ((codepoint >> 6) & 0x3F);
        buf[3] = 0x80 | (codepoint & 0x3F);
        return 4;
    }

    return 0;
}

unsigned int vui_utf8_to_cps(const char *str, uint32_t *out, unsigned int cap)
{
    VuiUtf8Decoder d;
    unsigned int n = 0;

    vui_utf8_reset(&d);

    for (; *str != '\0' && n < cap; str++) {
        if (vui_utf8_feed(&d, (uint8_t)*str, &out[n]) == VUI_UTF8_COMPLETE) {
            n++;
        }
    }

    return n;
}

size_t vui_cps_to_utf8(const uint32_t *cps, unsigned int n, char *out, size_t size)
{
    size_t used = 0;

    for (unsigned int i = 0; i < n; i++) {
        uint8_t buf[4];
        int len = vui_utf8_encode(cps[i], buf);

        if (used + len + 1 > size) {
            break;
        }

        for (int j = 0; j < len; j++) {
            out[used++] = (char)buf[j];
        }
    }

    if (size > 0) {
        out[used] = '\0';
    }

    return used;
}

// Terminal cell width: 0 for control and combining characters, 2 for East Asian
// wide / fullwidth and emoji blocks, 1 otherwise. A compact table, not full wcwidth().
int vui_cp_width(uint32_t cp)
{
    if (cp < 32 || (cp >= 0x7F && cp < 0xA0)) return 0;
    if (cp < 0x300) return 1;

    if ((cp >= 0x300 && cp <= 0x36F) || (cp >= 0x200B && cp <= 0x200F) ||
        (cp >= 0x20D0 && cp <= 0x20FF) || (cp >= 0xFE00 && cp <= 0xFE0F) || cp == 0xFEFF) {
        return 0;
    }

    if ((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF && cp != 0x303F) ||
        (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
        (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x1F300 && cp <= 0x1F64F) ||
        (cp >= 0x1F900 && cp <= 0x1F9FF) || (cp >= 0x20000 && cp <= 0x3FFFD)) {
        return 2;
    }

    return 1;
}

unsigned int vui_str_width(const char *str)
{
    return vui_strn_width(str, strlen(str));
}

unsigned int vui_strn_width(const char *str, size_t n)
{
    VuiUtf8Decoder d;
    uint32_t cp;
    unsigned int w = 0;

    vui_utf8_reset(&d);

    for (; n > 0; str++, n--) {
        if (vui_utf8_feed(&d, (uint8_t)*str, &cp) == VUI_UTF8_COMPLETE) {
            w += vui_cp_width(cp);
        }
    }

    return w;
}
