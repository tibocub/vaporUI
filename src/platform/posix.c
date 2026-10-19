#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#include "../../include/vaporui.h"
#include "../input.h"

// How long to wait for the rest of an escape sequence once a lone
// ESC byte has come in, before giving up and reporting a standalone
// Escape key. Same problem ncurses (ESCDELAY), vim (ttimeoutlen) and
// tmux (escape-time) all solve the same way: escape sequences from a
// real terminal arrive as a burst, not typed by hand, so a short
// bounded wait tells a real Escape keypress apart from the start of
// e.g. an arrow key without perceptibly delaying either one.
#define VUI_ESCAPE_TIMEOUT_MS 25

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

    // kitty keyboard protocol, 'disambiguate' level: lets terminals that
    // support it report Ctrl/Alt/Shift on any key. Others ignore it.
    fputs("\033[>1u\033[?2004h", stdout); // + bracketed paste
    fflush(stdout);

    return 0; 
}

void vui_platform_shutdown(void)
{
    fputs("\033[?1006l\033[?1002l\033[?1000l\033[?2004l\033[<u", stdout);
    fflush(stdout);

    tcsetattr(
        STDIN_FILENO,
        TCSAFLUSH,
        &original_termios
    );
}

void vui_platform_set_mouse(int enable)
{
    // 1000 clicks + wheel, 1002 drag motion, 1006 SGR encoding (no 223 col limit)
    fputs(enable ? "\033[?1000h\033[?1002h\033[?1006h" : "\033[?1006l\033[?1002l\033[?1000l", stdout);
    fflush(stdout);
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
    struct pollfd pfd;
    int timeout_ms;

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

    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    timeout_ms = vui_input_pending_escape(&input_decoder)
        ? VUI_ESCAPE_TIMEOUT_MS
        : -1;

    // Handles resize when poll is blocked
    if (poll(&pfd, 1, timeout_ms) == -1) {
        if (errno == EINTR && resize_pending) {
            VuiSize size;

            resize_pending = 0;

            if (vui_platform_get_size(&size) == 0) {
                event.type = VUI_EVENT_RESIZE;
                event.size = size;
            }
        }

        return event;
    }

    if (!(pfd.revents & POLLIN)) {
        // Timed out: no more bytes came, so a pending escape
        // sequence was actually just a standalone ESC keypress.
        return vui_input_timeout(&input_decoder);
    }

    result = read(STDIN_FILENO, &c, 1);

    if (result <= 0) {
        return event;
    }

    return vui_input_feed(&input_decoder, c);
}
