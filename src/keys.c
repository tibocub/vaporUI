#include <string.h>

#include "keys.h"

int vui_keys_reply_complete(const char *reply, size_t n)
{
    return n > 0 && reply[n - 1] == 'c';
}

VuiKeyMode vui_keys_mode(const char *reply, size_t n)
{
    if (!vui_keys_reply_complete(reply, n)) return VUI_KEYS_LEGACY;

    // the kitty reply is ESC [ ? <flags> u, the DA1 reply ESC [ ? ... c
    for (size_t i = 0; i + 3 <= n; i++) {
        if (memcmp(reply + i, "\033[?", 3) == 0) {
            size_t d = i + 3;

            while (d < n && reply[d] >= '0' && reply[d] <= '9') d++;
            if (d < n && reply[d] == 'u') return VUI_KEYS_KITTY;
        }
    }

    return VUI_KEYS_XTERM;
}

// kitty flags 1+4: disambiguate escape codes, report alternate keys
const char *vui_keys_enable(VuiKeyMode mode)
{
    return mode == VUI_KEYS_KITTY ? "\033[>5u" : mode == VUI_KEYS_XTERM ? "\033[>4;2m" : "";
}

const char *vui_keys_disable(VuiKeyMode mode)
{
    return mode == VUI_KEYS_KITTY ? "\033[<u" : mode == VUI_KEYS_XTERM ? "\033[>4;0m" : "";
}
