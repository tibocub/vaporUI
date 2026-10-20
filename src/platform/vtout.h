// The terminal-side half every backend shares: the escape sequences that switch modes,
// the clipboard sequence and color-depth detection. Backends handle the OS side (raw
// input, size, waiting).
#ifndef VAPORUI_VTOUT_H
#define VAPORUI_VTOUT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/vaporui.h"

// How long to wait for the rest of an escape sequence once a lone
// ESC byte has come in, before giving up and reporting a standalone
// Escape key. Same problem ncurses (ESCDELAY), vim (ttimeoutlen) and
// tmux (escape-time) all solve the same way: escape sequences from a
// real terminal arrive as a burst, not typed by hand, so a short
// bounded wait tells a real Escape keypress apart from the start of
// e.g. an arrow key without perceptibly delaying either one.
#define VUI_ESCAPE_TIMEOUT_MS 25

static inline void vtout_start(void)
{
    const char *ct = getenv("COLORTERM"), *term = getenv("TERM");

    // One write per frame: stdio on a console is unbuffered or line-buffered by default, which is
    // what made drags crawl on Windows.
    fflush(stdout);
    setvbuf(stdout, NULL, _IOFBF, 1 << 16);

    vui_set_color_depth(ct != NULL && (strstr(ct, "truecolor") || strstr(ct, "24bit")) ? 24
        : term != NULL && strstr(term, "256") ? 8 : 4);

    // kitty keyboard protocol ('disambiguate') and xterm modifyOtherKeys
    // level 1: let terminals that support either report Ctrl/Alt/Shift on
    // any key. Others ignore them.
    fputs("\033[?1049h\033[?25l\033[>1u\033[>4;1m\033[?2004h", stdout); // alternate screen (no scrollback pollution, terminals leave their scroll keys to the app), hide the cursor (widgets draw their own), + xterm modifyOtherKeys 1, bracketed paste
    fflush(stdout);
}

static inline void vtout_stop(void)
{
    fputs("\033[?25h\033[?1006l\033[?1003l\033[?1002l\033[?1000l\033[?2004l\033[>4;0m\033[<u\033[?1049l", stdout);
    fflush(stdout);
    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
}

static inline void vtout_set_mouse(int level)
{
    // 1000 clicks+wheel, 1002 drag motion, 1003 all motion, 1006 SGR encoding (no 223 col limit)
    fputs("\033[?1003l\033[?1002l\033[?1000l", stdout);

    if (level > 0) {
        fputs(level > 1 ? "\033[?1003h\033[?1006h" : "\033[?1002h\033[?1000h\033[?1006h", stdout);
    }

    fflush(stdout);
}

static inline void vtout_osc52_copy(const char *utf8)
{
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t n = strlen(utf8);

    fputs("\033]52;c;", stdout);

    for (size_t i = 0; i < n; i += 3) {
        unsigned int v = (unsigned char)utf8[i] << 16;

        if (i + 1 < n) v |= (unsigned char)utf8[i + 1] << 8;
        if (i + 2 < n) v |= (unsigned char)utf8[i + 2];

        putchar(b64[v >> 18]);
        putchar(b64[(v >> 12) & 63]);
        putchar(i + 1 < n ? b64[(v >> 6) & 63] : '=');
        putchar(i + 2 < n ? b64[v & 63] : '=');
    }

    fputs("\007", stdout);
    fflush(stdout);
}

#endif
