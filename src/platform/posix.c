#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "../../include/vaporui.h"
#include "../input.h"
#include "../ctx.h"

// How long to wait for the rest of an escape sequence once a lone
// ESC byte has come in, before giving up and reporting a standalone
// Escape key. Same problem ncurses (ESCDELAY), vim (ttimeoutlen) and
// tmux (escape-time) all solve the same way: escape sequences from a
// real terminal arrive as a burst, not typed by hand, so a short
// bounded wait tells a real Escape keypress apart from the start of
// e.g. an arrow key without perceptibly delaying either one.
#define VUI_ESCAPE_TIMEOUT_MS 25

static void osc52_copy(const char *utf8);

static struct termios original_termios;
static int tty_fd = STDIN_FILENO;   // the terminal: stdin, or /dev/tty when stdin is piped
static FILE *saved_stdout;          // the real stdout while the UI draws on /dev/tty
static volatile sig_atomic_t resize_pending = 0;
static VuiInputDecoder input_decoder;

static void handle_sigwinch(int signal)
{
    (void)signal;
    resize_pending = 1;
}

static void release_tty(void)
{
    if (saved_stdout != NULL) {
        fclose(stdout);
        close(tty_fd);
        stdout = saved_stdout;
        saved_stdout = NULL;
        tty_fd = STDIN_FILENO;
    }
}

int vui_platform_init(void)
{
    struct termios raw;
    struct sigaction action;

    // Filters (`find | picker`): keys come from the terminal, not the pipe, and the
    // UI is drawn on the terminal, leaving stdout free for the program's result.
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        FILE *out;

        tty_fd = open("/dev/tty", O_RDWR);
        out = tty_fd >= 0 ? fdopen(dup(tty_fd), "w") : NULL;

        if (out == NULL) {
            return -1;
        }

        saved_stdout = stdout;
        fflush(stdout);
        stdout = out;
    }

    if (tcgetattr(tty_fd, &original_termios) == -1) {
        release_tty();
        return -1;
    }

    raw = original_termios;

    // ICANON	-> don't wait for the user to press 'enter'
    // ECHO		-> don't print in term the pressed keys
    // ISIG		-> don't interpret keys, only pass them (for
    //           example, 'ctrl-c' sends 0x03, not SIGINT)
    // IEXTEN -> don't swallow Ctrl-V (literal-next); IXON -> Ctrl-S / Ctrl-Q are keys, not flow control
    raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
    raw.c_iflag &= ~IXON;

    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(tty_fd, TCSAFLUSH, &raw) == -1) {
        release_tty();
        return -1;
    }

    action.sa_handler = handle_sigwinch;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGWINCH, &action, NULL) == -1) {
        tcsetattr(tty_fd, TCSAFLUSH, &original_termios);
        release_tty();
        return -1;
    }

    {
        const char *ct = getenv("COLORTERM"), *term = getenv("TERM");

        vui_set_color_depth(ct != NULL && (strstr(ct, "truecolor") || strstr(ct, "24bit")) ? 24
            : term != NULL && strstr(term, "256") ? 8 : 4);
    }

    vui_input_reset(&input_decoder);
    vui_clipboard_hook = osc52_copy;

    // kitty keyboard protocol ('disambiguate') and xterm modifyOtherKeys
    // level 1: let terminals that support either report Ctrl/Alt/Shift on
    // any key. Others ignore them.
    fputs("\033[?1049h\033[?25l\033[>1u\033[>4;1m\033[?2004h", stdout); // alternate screen (no scrollback pollution, terminals leave their scroll keys to the app), hide the cursor (widgets draw their own), + xterm modifyOtherKeys 1, bracketed paste
    fflush(stdout);

    return 0; 
}

void vui_platform_shutdown(void)
{
    fputs("\033[?25h\033[?1006l\033[?1003l\033[?1002l\033[?1000l\033[?2004l\033[>4;0m\033[<u\033[?1049l", stdout);
    fflush(stdout);

    tcsetattr(
        tty_fd,
        TCSAFLUSH,
        &original_termios
    );

    release_tty();
}

static void osc52_copy(const char *utf8)
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

void vui_platform_set_mouse(int level)
{
    // 1000 clicks+wheel, 1002 drag motion, 1003 all motion, 1006 SGR encoding (no 223 col limit)
    fputs("\033[?1003l\033[?1002l\033[?1000l", stdout);

    if (level > 0) {
        fputs(level > 1 ? "\033[?1003h\033[?1006h" : "\033[?1002h\033[?1000h\033[?1006h", stdout);
    }

    fflush(stdout);
}

int vui_platform_get_size(VuiSize *size)
{
    struct winsize ws;

    if (size == NULL) {
        return -1;
    }

    if (ioctl(tty_fd, TIOCGWINSZ, &ws) == -1) {
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

uint64_t vui_platform_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Waits up to timeout_ms (-1 = forever) for terminal input or any of the
// n extra fds; ready[i] is set for the fds that are readable.
VuiEvent vui_platform_poll_event(int timeout_ms, const int *fds, unsigned int n, unsigned char *ready)
{
    unsigned char c;
    ssize_t result;
    struct pollfd pfd[1 + VUI_MAX_WATCH];
    int esc = vui_input_pending_escape(&input_decoder), esc_first;

    VuiEvent event = {
        .type = VUI_EVENT_NONE
    };

    memset(ready, 0, n);

    if (resize_pending) {
        VuiSize size;

        resize_pending = 0;

        if (vui_platform_get_size(&size) == 0) {
            event.type = VUI_EVENT_RESIZE;
            event.size = size;
        }

        return event;
    }

    pfd[0].fd = tty_fd;
    pfd[0].events = POLLIN;

    for (unsigned int i = 0; i < n; i++) {
        pfd[1 + i].fd = fds[i];
        pfd[1 + i].events = POLLIN;
    }

    esc_first = esc && (timeout_ms < 0 || VUI_ESCAPE_TIMEOUT_MS <= timeout_ms);

    if (esc_first) {
        timeout_ms = VUI_ESCAPE_TIMEOUT_MS;
    }

    // Handles resize when poll is blocked
    result = poll(pfd, 1 + n, timeout_ms);

    if (result == -1) {
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

    for (unsigned int i = 0; i < n; i++) {
        ready[i] = (pfd[1 + i].revents & (POLLIN | POLLHUP | POLLERR)) != 0;
    }

    if (!(pfd[0].revents & POLLIN)) {
        // Timed out: no more bytes came, so a pending escape
        // sequence was actually just a standalone ESC keypress.
        return result == 0 && esc_first ? vui_input_timeout(&input_decoder) : event;
    }

    if (read(tty_fd, &c, 1) <= 0) {
        return event;
    }

    return vui_input_feed(&input_decoder, c);
}
