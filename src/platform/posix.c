#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../../include/vaporui.h"

int vui_platform_get_size(VuiSize *size)
{
    struct winsize ws;

    if (size == NULL) {
        return -1;
    }

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return -1;
    }

    size->width = ws.ws_col;
    size->height = ws.ws_row;

    return 0;
}

void vui_platform_clear(void)
{
    // Clear entire display
    fputs("\033[2J", stdout);

    // Move cursor to home position (1, 1)
    fputs("\033[H", stdout);

    // fflush() forces the buffered characters
    // to be written to the terminal now.
    fflush(stdout);
}
