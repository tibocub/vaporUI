// Key negotiation parsing (pure). `make test`.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "keys.h"

int main(void)
{
    size_t a, b;
    const char *kitty = "ab\033[?0u\033[?62;4cXY";

    assert(vui_keys_find_reply(kitty, strlen(kitty), &a, &b) && a == 2 && b == strlen(kitty) - 2); // typing before and after survives
    assert(vui_keys_mode(kitty + a, b - a) == VUI_KEYS_KITTY);
    assert(!vui_keys_find_reply("\033[?0u", 5, &a, &b));          // DA1 not there yet
    assert(!vui_keys_find_reply("x\033[?6", 5, &a, &b));          // cut in the middle
    assert(vui_keys_find_reply("\033[?62;4c", 8, &a, &b) && vui_keys_mode("\033[?62;4c", 8) == VUI_KEYS_XTERM);
    assert(!vui_keys_find_reply("\033[?1;x2c", 8, &a, &b));       // not made of parameter bytes: not a reply
    puts("keys: ok");
    return 0;
}
