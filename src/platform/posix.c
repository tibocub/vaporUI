#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "../../include/vaporui.h"

static struct termios original_termios;

int vui_platform_init(void)
{
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &original_termios) == -1) {
        return -1;
    }

    raw = original_termios;

    // Don't wait for Enter, get input immediatly.
    raw.c_lflag &= ~(ICANON | ECHO);

    // Receives Ctrl-C as the byte 0x03 instead
    // of having the terminal generate SIGINT.
    raw.c_lflag &= ~ISIG;

    // Read one byte at a time.
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return -1;
    }

    return 0;
}

void vui_platform_shutdown(void)
{
    tcsetattr(
        STDIN_FILENO,
        TCSAFLUSH,
        &original_termios
    );
}

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
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
}

VuiEvent vui_platform_poll_event(void)
{
    unsigned char c;
    ssize_t result;

    VuiEvent event = {
        .type = VUI_EVENT_NONE
    };

    do {
        result = read(STDIN_FILENO, &c, 1);
    } while (result == -1 && errno == EINTR);

    if (result <= 0) {
        return event;
    }

    event.type = VUI_EVENT_KEY;

    switch (c) {
    case 3:
        event.key.key = VUI_KEY_CTRL_C;
        event.key.character = c;
        break;

    case '\n':
    case '\r':
        event.key.key = VUI_KEY_ENTER;
        event.key.character = c;
        break;

    case '\t':
        event.key.key = VUI_KEY_TAB;
        event.key.character = c;
        break;

    case 27:
        event.key.key = VUI_KEY_ESCAPE;
        event.key.character = c;
        break;

    case 127:
        event.key.key = VUI_KEY_BACKSPACE;
        event.key.character = c;
        break;

    default:
        event.key.key = VUI_KEY_CHAR;
        event.key.character = c;
        break;
    }

    return event;
}
