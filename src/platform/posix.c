#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#include "../../include/vaporui.h"
#include "../input.h"

static struct termios original_termios;
static volatile sig_atomic_t resize_pending = 0;
static VuiInputDecoder input_decoder;

static void handle_sigwinch(int signal)
{
    (void)signal;
    resize_pending = 1;
}

int vui_platform_init(void)
{
    struct termios raw;
    struct sigaction action;

    if (tcgetattr(STDIN_FILENO, &original_termios) == -1) {
        return -1;
    }

    raw = original_termios;

    // ICANON	-> don't wait for the user to press 'enter'
    // ECHO		-> don't print in term the pressed keys
    // ISIG		-> don't interpret keys, only pass them (for
    //           example, 'ctrl-c' sends 0x03, not SIGINT)
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);

    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return -1;
    }

    action.sa_handler = handle_sigwinch;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGWINCH, &action, NULL) == -1) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
        return -1;
    }

    vui_input_reset(&input_decoder);

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

    if (resize_pending) {
        VuiSize size;

        resize_pending = 0;

        if (vui_platform_get_size(&size) == 0) {
            event.type = VUI_EVENT_RESIZE;
            event.size = size;
        }

        return event;
    }

    result = read(STDIN_FILENO, &c, 1);

    // Handles resize when read is blocked
    if (result == -1) {
        if (errno == EINTR && resize_pending) {
            VuiSize size;

            resize_pending = 0;

            if (vui_platform_get_size(&size) == 0) {
                event.type = VUI_EVENT_RESIZE;
                event.size = size;
            }

            return event;
        }

        return event;
    }

    if (result == 0) {
        return event;
    }

    return vui_input_feed(&input_decoder, c);
}
