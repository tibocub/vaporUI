#ifndef VAPORUI_KEYS_H
#define VAPORUI_KEYS_H

#include <stddef.h>

// Negotiating extended key reporting with a terminal (Ctrl/Alt on any key, the physical key), the way neovim does.
// Pure strings and parsing, no I/O: the backend sends VUI_KEYS_QUERY, collects the reply until
// vui_keys_reply_complete(), asks vui_keys_mode() what the terminal said, writes vui_keys_enable() and, at exit,
// vui_keys_disable(). Kitty if it answered the query, else xterm modifyOtherKeys level 2, never both.

typedef enum { VUI_KEYS_LEGACY, VUI_KEYS_KITTY, VUI_KEYS_XTERM } VuiKeyMode;

// kitty keyboard flags query, then Primary Device Attributes, which every terminal answers: when DA1 arrives and
// no kitty reply came before it, the terminal does not speak kitty
#define VUI_KEYS_QUERY "\033[?u\033[c"

int vui_keys_reply_complete(const char *reply, size_t n); // the DA1 reply (ends in 'c') has arrived
VuiKeyMode vui_keys_mode(const char *reply, size_t n);    // n == 0: no answer at all, legacy

const char *vui_keys_enable(VuiKeyMode mode);
const char *vui_keys_disable(VuiKeyMode mode);

#endif
