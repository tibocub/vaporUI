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
#ifdef __linux__
#include <linux/kd.h>
#include <linux/tiocl.h>
#include <linux/vt.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#endif

#include "../../include/vaporui.h"
#include "../input.h"
#include "../keys.h"
#include "../vtraw.h"
#include "../gpm.h"
#include "../ctx.h"
#include "vtout.h"

static struct termios original_termios;
static int tty_fd = STDIN_FILENO;   // the terminal: stdin, or /dev/tty when stdin is piped
static FILE *saved_stdout;          // the real stdout while the UI draws on /dev/tty
static volatile sig_atomic_t resize_pending = 0;
static VuiInputDecoder input_decoder;

// Bytes read from the terminal but not yet decoded: one read() per burst instead of one per byte
// (a paste is thousands of bytes; on a small target the syscalls were the cost).
static unsigned char rx[64];
static unsigned int rx_len, rx_pos;

// Extended keys: the strings and the decision are in keys.c; this reads the terminal's reply.
// shortcut: a terminal that answers nothing costs 100 ms at start (every other one answers in a few ms). Keys typed
// meanwhile are kept (put back in rx), not lost.
static VuiKeyMode key_mode;

static void keys_start(void)
{
    char buf[128];
    size_t n = 0, a = 0, b = 0;
    ssize_t got;
    struct pollfd pfd = { .fd = tty_fd, .events = POLLIN };

    fputs(VUI_KEYS_QUERY, stdout);
    fflush(stdout);

    while (n < sizeof(buf) && poll(&pfd, 1, 100) > 0 && (got = read(tty_fd, buf + n, sizeof(buf) - n)) > 0) {
        n += (size_t)got;
        if (vui_keys_find_reply(buf, n, &a, &b)) break;
    }

    if (vui_keys_find_reply(buf, n, &a, &b)) {
        key_mode = vui_keys_mode(buf + a, b - a);
        memmove(buf + a, buf + b, n - b); // what is left is the user's typing
        n -= b - a;
    } else {
        key_mode = VUI_KEYS_LEGACY;
    }

    for (size_t i = 0; i < n && rx_len < sizeof(rx); i++) rx[rx_len++] = (unsigned char)buf[i];

    fputs(vui_keys_enable(key_mode), stdout);
    fflush(stdout);
}

static void keys_stop(void)
{
    fputs(vui_keys_disable(key_mode), stdout);
    key_mode = VUI_KEYS_LEGACY;
}

#ifdef __linux__
// The Linux console sends Alt+e-acute and Alt+Shift+e-acute as the same ESC 2 (the keymap says Meta_two for both), so
// ask the console which modifiers are down right now (what the `shiftstate` utility does).
// shortcut: Shift released before we ask (a few microseconds after the key arrived) reads as not held.
static int vt_shift(void)
{
    char state = TIOCL_GETSHIFTSTATE;

    if (ioctl(tty_fd, TIOCLINUX, &state) < 0) return -1;

    // kernel bits: 1 shift, 2 altgr, 4 ctrl, 8 alt
    return ((state & 1) ? VUI_MOD_SHIFT : 0) | ((state & 4) ? VUI_MOD_CTRL : 0) | ((state & 8) ? VUI_MOD_ALT : 0);
}
#endif

static void handle_sigwinch(int signal)
{
    (void)signal;
    resize_pending = 1;
}

// NuttX's stdout is a function call, not an assignable stream: there the UI always uses stdin/stdout.
#ifndef __NuttX__
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
#else
static void release_tty(void) {}
#endif

#ifdef __linux__
// The Linux console, raw: the kernel's keymap loses information (Ctrl+é, Alt+Shift+é...), so on a real VT the
// keyboard is put in K_MEDIUMRAW, which reports physical keys, and src/vtraw.c does the translation with the
// kernel's own keymap. Safety: the mode is per console and survives our exit if we die without restoring it, so
// (1) it is restored at exit and on fatal signals, (2) a small guard process restores it if we are killed outright,
// (3) Ctrl+Alt+F<n> switches console like the kernel did. VUI_RAW_KEYS=0 turns it all off.
// shortcut: no LED updates for CapsLock/NumLock, no compose key, no AltGr+dead-key sequences beyond the accent table.
static VuiVtRaw *vtraw;
static int on_vt; // stdin is a real Linux console
static long vt_saved_mode;
static int guard_pipe = -1;
static pid_t guard_pid;

static void vt_restore_mode(void)
{
    if (vtraw != NULL) ioctl(tty_fd, KDSKBMODE, vt_saved_mode);
}

static void vt_fatal(int sig)
{
    signal(sig, SIG_DFL);
    vt_restore_mode();
    raise(sig);
}

static int vt_load_keymap(VuiVtRaw *raw)
{
    struct kbdiacrsuc *diacr = calloc(1, sizeof(*diacr));
    int chars = 0;

    for (int t = 0; t < VUI_VT_TABLES; t++) {
        for (int k = 0; k < VUI_VT_KEYS; k++) {
            struct kbentry ent = { .kb_table = (unsigned char)t, .kb_index = (unsigned char)k };

            if (ioctl(tty_fd, KDGKBENT, &ent) != 0) { free(diacr); return -1; }
            raw->map[t][k] = vui_vtraw_entry(ent.kb_value);
            chars += t == 0 && (raw->map[t][k].type == VT_CHAR || raw->map[t][k].type == VT_LETTER);
        }
    }

    raw->accents = 0;
    if (diacr != NULL && ioctl(tty_fd, KDGKBDIACRUC, diacr) == 0) {
        for (unsigned int i = 0; i < diacr->kb_cnt && i < VUI_VT_DIACR; i++) {
            raw->accent[i] = (VuiVtAccent){ diacr->kbdiacruc[i].diacr, diacr->kbdiacruc[i].base, diacr->kbdiacruc[i].result };
            raw->accents = i + 1;
        }
    }

    free(diacr);
    return chars >= 20 ? 0 : -1; // a map with no letters is not a keymap we understand
}

static unsigned char led_shown; // what the keyboard LEDs show: in raw mode the app owns CapsLock / NumLock

static void vt_raw_stop(void)
{
    if (vtraw == NULL) return;

    ioctl(tty_fd, KDSETLED, 0xFF); // the kernel's own LED handling again
    vt_restore_mode();
    free(vtraw);
    vtraw = NULL;

    if (guard_pipe >= 0) { // tell the guard we are done, it exits
        close(guard_pipe);
        guard_pipe = -1;
        waitpid(guard_pid, NULL, 0);
    }
}

static void vt_raw_start(struct termios *raw_termios)
{
    char kbtype;
    int fds[2];
    char led = 0;
    const char *off = getenv("VUI_RAW_KEYS");
    struct termios t;
    static const int fatal[] = { SIGTERM, SIGHUP, SIGINT, SIGQUIT, SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL };

    if (ioctl(tty_fd, KDGKBTYPE, &kbtype) != 0) return; // not a real console: pty, ssh, serial line
    on_vt = 1;

    if (off != NULL && off[0] == '0') { // asked for the old way: Alt+é etc. recovered with the Shift probe
        vui_input_set_shift_probe(vt_shift);
        return;
    }

    if (ioctl(tty_fd, KDGKBMODE, &vt_saved_mode) != 0 || (vt_saved_mode != K_XLATE && vt_saved_mode != K_UNICODE)) return;

    vtraw = calloc(1, sizeof(*vtraw));
    if (vtraw == NULL || vt_load_keymap(vtraw) != 0 || pipe(fds) != 0) {
        free(vtraw);
        vtraw = NULL;
        vui_input_set_shift_probe(vt_shift);
        return;
    }

    ioctl(tty_fd, KDGKBLED, &led);
    vui_vtraw_init(vtraw, (led & LED_NUM) != 0);
    led_shown = led & (LED_NUM | LED_CAP);

    guard_pid = fork();
    if (guard_pid == 0) { // the guard: restores the mode when the pipe closes, however we died
        char c;

        close(fds[1]);
        setpgid(0, 0);
        for (unsigned int i = 0; i < sizeof(fatal) / sizeof(fatal[0]); i++) signal(fatal[i], SIG_IGN);
        while (read(fds[0], &c, 1) > 0) {}
        ioctl(tty_fd, KDSKBMODE, vt_saved_mode);
        _exit(0);
    }

    close(fds[0]);
    if (guard_pid < 0) { // no guard, no raw mode
        close(fds[1]);
        free(vtraw);
        vtraw = NULL;
        vui_input_set_shift_probe(vt_shift);
        return;
    }

    fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    guard_pipe = fds[1];

    for (unsigned int i = 0; i < sizeof(fatal) / sizeof(fatal[0]); i++) signal(fatal[i], vt_fatal);

    // keycode bytes are not text: no input translation at all (kbd's showkey does the same)
    t = *raw_termios;
    t.c_iflag = 0;
    tcsetattr(tty_fd, TCSANOW, &t);
    ioctl(tty_fd, KDSKBMODE, K_MEDIUMRAW);
}
#endif

static int vt_queued(void)
{
#ifdef __linux__
    return vtraw != NULL && vtraw->queue_len > 0;
#else
    return 0;
#endif
}

static VuiEvent vt_pop(void)
{
#ifdef __linux__
    if (vtraw != NULL) return vui_vtraw_pop(vtraw);
#endif
    return (VuiEvent){ .type = VUI_EVENT_NONE };
}

static VuiEvent vt_feed(unsigned char byte)
{
#ifdef __linux__
    if (vtraw != NULL) {
        VuiEvent e = vui_vtraw_feed(vtraw, byte);
        unsigned char leds = (vtraw->caps ? LED_CAP : 0) | (vtraw->num ? LED_NUM : 0);

        if (leds != led_shown) {
            ioctl(tty_fd, KDSETLED, leds);
            led_shown = leds;
        }

        if (vtraw->switch_to != 0) {
            ioctl(tty_fd, VT_ACTIVATE, vtraw->switch_to);
            vtraw->switch_to = 0;
            vui_vtraw_release_all(vtraw); // the releases of Ctrl and Alt go to the other console
        }

        return e;
    }
#endif
    return vui_input_feed(&input_decoder, byte);
}

#ifdef __linux__
// The keyboard layout, for terminals that can't say which key was pressed (the Linux console in raw mode doesn't
// need it). First hit wins; a desktop session asks the desktop, a console reads the system files.
// Settings are read once at start (a few fopen; the desktop tools are only run in a graphical session).
// shortcut: a layout switched while the app runs is not noticed; KDE on Wayland is not asked.
static char layout_name[32];

static int scan(FILE *f, const char *prefix, char open, char close)
{
    char line[256];
    int found = 0;

    if (f == NULL) return 0;
    while (!found && fgets(line, sizeof(line), f) != NULL) {
        char *at = strstr(line, prefix), *end;

        if (at == NULL) continue;
        at += strlen(prefix);
        if (open != 0 && (at = strchr(at, open)) == NULL) continue;
        at += open != 0;
        while (*at == ' ' || *at == '\t' || *at == '"') at++;
        end = at + strcspn(at, close ? (char[]){ close, '\n', '"', ' ', '\0' } : "\n\" ");
        if (end > at && (size_t)(end - at) < sizeof(layout_name)) {
            memcpy(layout_name, at, (size_t)(end - at));
            layout_name[end - at] = '\0';
            found = 1;
        }
    }
    return found;
}

// system files are read directly; only the two desktop tools are run
static int match_file(const char *path, const char *prefix, char open, char close)
{
    FILE *f = fopen(path, "r");
    int found = scan(f, prefix, open, close);

    if (f != NULL) fclose(f);
    return found;
}

static int match_cmd(const char *cmd, const char *prefix, char open, char close)
{
    FILE *f = popen(cmd, "r");
    int found = scan(f, prefix, open, close);

    if (f != NULL) pclose(f);
    return found;
}

static const char *detect_layout(void)
{
    const char *env = getenv("XKB_DEFAULT_LAYOUT");
    int desktop = getenv("DISPLAY") != NULL || getenv("WAYLAND_DISPLAY") != NULL;

    if (env != NULL && *env != '\0') return env;

    if (desktop && (match_cmd("gsettings get org.gnome.desktop.input-sources sources 2>/dev/null", "'xkb'", '\'', '\'')
                    || match_cmd("setxkbmap -query 2>/dev/null", "layout:", 0, 0))) {
        return layout_name;
    }

    if (match_file("/etc/vconsole.conf", "KEYMAP=", 0, 0)) {
        if (strcmp(layout_name, "uk") == 0) strcpy(layout_name, "gb"); // the console's name for British
        return layout_name;
    }

    if (match_file("/etc/default/keyboard", "XKBLAYOUT=", 0, 0)
        || match_file("/etc/X11/xorg.conf.d/00-keyboard.conf", "\"XkbLayout\"", '"', '"')) {
        return layout_name;
    }

    return NULL;
}
#endif

#ifdef __linux__
// Mouse on the Linux console: the gpm daemon, if it runs, owns the mouse (it draws a pointer and selects text on the
// console itself). While an app wants the mouse we connect to /dev/gpmctl and take the events (src/gpm.c), which
// switches the daemon's own selection off for us; closing the socket gives it back. No daemon, no mouse.
// The daemon draws no pointer for a client that takes every event (and a client may only draw one as root), so the
// frame draws its own (vui_soft_pointer). shortcut: plain motion events reach the app even at mouse level 1.
static int gpm_fd = -1, gpm_level;
static unsigned char gpm_buf[VUI_GPM_EVENT_SIZE];
static VuiGpmState gpm_state;
static VuiEvent gpm_pending = { .type = VUI_EVENT_NONE }; // the second event of a middle click
static unsigned int gpm_have;

static void gpm_close(void)
{
    if (gpm_fd >= 0) {
        close(gpm_fd);
        gpm_fd = -1;
    }
    gpm_have = 0;
    vui_soft_pointer = 0;
}

static void gpm_open(void)
{
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    unsigned char hello[VUI_GPM_HELLO_SIZE];
    const char *name = ttyname(tty_fd);
    int vc = 0;

    if (gpm_fd >= 0 || !on_vt) return;
    if (name != NULL) {
        const char *digits = name + strlen(name);

        while (digits > name && digits[-1] >= '0' && digits[-1] <= '9') digits--;
        vc = atoi(digits);
    }
    if (vc <= 0) return;

    gpm_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (gpm_fd < 0) return;

    strcpy(addr.sun_path, "/dev/gpmctl");
    vui_gpm_hello(hello, (int)getpid(), vc);
    if (connect(gpm_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 || write(gpm_fd, hello, sizeof(hello)) != (ssize_t)sizeof(hello)) {
        close(gpm_fd);
        gpm_fd = -1;
        return;
    }
    fcntl(gpm_fd, F_SETFD, FD_CLOEXEC);
    {
        const char *u = getenv("VUI_GPM_SCROLL"); // movement per wheel step of the middle-button lever; 0 turns it off

        vui_gpm_state_init(&gpm_state, u != NULL ? atoi(u) : 10);
    }
    vui_soft_pointer = 1; // the daemon draws no pointer for a client that takes every event: the frame draws one
}

// One event from the daemon, or NONE
static VuiEvent gpm_read(void)
{
    VuiEvent event = { .type = VUI_EVENT_NONE };
    ssize_t n = read(gpm_fd, gpm_buf + gpm_have, sizeof(gpm_buf) - gpm_have);

    if (n <= 0) { // the daemon went away
        if (n == 0 || (errno != EINTR && errno != EAGAIN)) gpm_close();
        return event;
    }

    gpm_have += (unsigned int)n;
    if (gpm_have < sizeof(gpm_buf)) return event;
    gpm_have = 0;

    VuiEvent two[2];

    if (vui_gpm_event(gpm_buf, 2, &gpm_state, two) == 0) return event; // always with motion: the pointer follows it
    gpm_pending = two[1].type == VUI_EVENT_MOUSE ? two[1] : (VuiEvent){ .type = VUI_EVENT_NONE };
    return two[0];
}
#endif

int vui_platform_init(void)
{
    struct termios raw;
    struct sigaction action;

    // Filters (`find | picker`): keys come from the terminal, not the pipe, and the
    // UI is drawn on the terminal, leaving stdout free for the program's result.
#ifndef __NuttX__
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
#endif

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

#ifdef __linux__
    vui_layout_hook = detect_layout;
#endif
    vui_input_reset(&input_decoder);
    rx_len = rx_pos = 0;
    vui_clipboard_hook = vtout_osc52_copy;
    vtout_start();
#ifdef __linux__
    on_vt = 0;
    vt_raw_start(&raw);
    if (vtraw == NULL)
#endif
    keys_start();
#ifdef __linux__
    if (vtraw == NULL) {
        char kbtype; // only a real VT answers this (not a pty, ssh, or a serial line)

        vui_input_set_shift_probe(ioctl(tty_fd, KDGKBTYPE, &kbtype) == 0 ? vt_shift : NULL);
    }
#endif

    return 0; 
}

void vui_platform_shutdown(void)
{
#ifdef __linux__
    gpm_close();
    vt_raw_stop();
    if (key_mode != VUI_KEYS_LEGACY)
#endif
    keys_stop();
    vui_input_set_shift_probe(NULL);
    vtout_stop();

    tcsetattr(
        tty_fd,
        TCSAFLUSH,
        &original_termios
    );

    release_tty();
}

void vui_platform_set_mouse(int level)
{
    vtout_set_mouse(level);
#ifdef __linux__
    gpm_level = level;
    if (level > 0) gpm_open();
    else gpm_close();
#endif
}

// Serial consoles (NuttX, a bare UART) have no window size: park the cursor in the far corner and
// ask the terminal where it ended up. shortcut: swallows any key typed during the 300 ms wait.
static int probe_size(VuiSize *size)
{
    char buf[24] = { 0 };
    unsigned int n = 0, rows, cols;
    struct pollfd pfd = { .fd = tty_fd, .events = POLLIN };

    fputs("\033[s\033[9999;9999H\033[6n\033[u", stdout);
    fflush(stdout);

    while (n < sizeof(buf) - 1 && poll(&pfd, 1, 300) > 0 && read(tty_fd, buf + n, 1) == 1) {
        if (buf[n++] == 'R') {
            buf[n] = '\0';
            break;
        }
    }

    char *reply = strstr(buf, "\033[");

    if (reply == NULL || sscanf(reply, "\033[%u;%uR", &rows, &cols) != 2 || rows == 0 || cols == 0) {
        return -1;
    }

    size->width = cols;
    size->height = rows;
    return 0;
}

int vui_platform_get_size(VuiSize *size)
{
    struct winsize ws;

    if (size == NULL) {
        return -1;
    }

    if (ioctl(tty_fd, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0 || ws.ws_row == 0) {
        return probe_size(size);
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

// True when a read of the terminal wouldn't block: the loop skips drawing frames that newer input
// would immediately replace (drags, key repeat).
int vui_platform_input_pending(void)
{
    struct pollfd pfd = { .fd = tty_fd, .events = POLLIN };

    return resize_pending || rx_pos < rx_len || vt_queued() || (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN));
}

// Waits up to timeout_ms (-1 = forever) for terminal input or any of the
// n extra fds; ready[i] is set for the fds that are readable.
VuiEvent vui_platform_poll_event(int timeout_ms, const int *fds, unsigned int n, unsigned char *ready)
{
    ssize_t result;
    struct pollfd pfd[2 + VUI_MAX_WATCH];
    int has_gpm = 0;
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

    {
        VuiEvent queued = vt_pop();

        if (queued.type != VUI_EVENT_NONE) return queued;
    }

    if (rx_pos < rx_len) {
        return vt_feed(rx[rx_pos++]);
    }

    pfd[0].fd = tty_fd;
    pfd[0].events = POLLIN;

    for (unsigned int i = 0; i < n; i++) {
        pfd[1 + i].fd = fds[i];
        pfd[1 + i].events = POLLIN;
    }

#ifdef __linux__
    if (gpm_pending.type != VUI_EVENT_NONE) {
        event = gpm_pending;
        gpm_pending.type = VUI_EVENT_NONE;
        return event;
    }
    if (gpm_fd >= 0) {
        pfd[1 + n].fd = gpm_fd;
        pfd[1 + n].events = POLLIN;
        has_gpm = 1;
    }
#endif

    esc_first = esc && (timeout_ms < 0 || VUI_ESCAPE_TIMEOUT_MS <= timeout_ms);

    if (esc_first) {
        timeout_ms = VUI_ESCAPE_TIMEOUT_MS;
    }

    // Handles resize when poll is blocked
    result = poll(pfd, 1 + n + has_gpm, timeout_ms);

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

#ifdef __linux__
    if (has_gpm && (pfd[1 + n].revents & (POLLIN | POLLHUP | POLLERR))) {
        VuiEvent mouse = gpm_read();

        if (mouse.type != VUI_EVENT_NONE) return mouse;
    }
#endif

    if (pfd[0].revents & (POLLHUP | POLLERR | POLLNVAL) && !(pfd[0].revents & POLLIN)) {
        event.type = VUI_EVENT_CLOSE;
        return event;
    }

    if (!(pfd[0].revents & POLLIN)) {
        // Timed out: no more bytes came, so a pending escape
        // sequence was actually just a standalone ESC keypress.
        return result == 0 && esc_first ? vui_input_timeout(&input_decoder) : event;
    }

    result = read(tty_fd, rx, sizeof(rx));

    if (result == 0 || (result < 0 && errno != EINTR && errno != EAGAIN)) {
        event.type = VUI_EVENT_CLOSE;
        return event;
    }

    if (result < 0) {
        return event;
    }

    rx_len = (unsigned int)result;
    rx_pos = 0;

    return vt_feed(rx[rx_pos++]);
}
