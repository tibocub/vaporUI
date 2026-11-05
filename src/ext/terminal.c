// Terminal widget: a child process on a pty, its output interpreted by libvterm (third_party/libvterm).
// POSIX (forkpty) and NuttX (/dev/ptmx + posix_spawn: no MMU, no fork); not part of the default
// build, see the Makefile and vaporOS/vaporui.
#ifndef __NuttX__
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#endif
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__NuttX__)
#include <fcntl.h>
#include <spawn.h>
#elif defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

#include "../../include/vaporui.h"
#include "../../third_party/libvterm/include/vterm.h"

static VuiColor color(VTermScreen *screen, VTermColor c)
{
    if (VTERM_COLOR_IS_DEFAULT_FG(&c) || VTERM_COLOR_IS_DEFAULT_BG(&c)) return VUI_COLOR_DEFAULT;
    if (VTERM_COLOR_IS_INDEXED(&c)) return c.indexed.idx < 16 ? (VuiColor)c.indexed.idx + 1 : VUI_COLOR256(c.indexed.idx);
    vterm_screen_convert_color_to_rgb(screen, &c);
    return VUI_RGB(c.rgb.red, c.rgb.green, c.rgb.blue);
}

static void term_draw(VuiNode *node, VuiScreen *scr, void *userdata)
{
    VuiTerminal *t = (VuiTerminal *)node;
    VuiRect r = node->rect;
    VTermPos cur;
    VuiStyle blank = vui_theme()->text;

    (void)userdata;

    // follow the node's size: the vterm and the child's idea of the window must match what we show
    if (r.width > 0 && r.height > 0 && (r.width != t->cols || r.height != t->rows)) {
        struct winsize ws = { .ws_row = r.height, .ws_col = r.width };

        t->cols = r.width;
        t->rows = r.height;
        vterm_set_size(t->vt, t->rows, t->cols);
        if (t->fd >= 0) ioctl(t->fd, TIOCSWINSZ, &ws);
    }

    scr_fill(scr, r, ' ', blank);

    for (unsigned int y = 0; y < r.height && y < t->rows; y++) {
        for (unsigned int x = 0; x < r.width && x < t->cols; x++) {
            VTermScreenCell c;
            VuiStyle s = { 0 };

            vterm_screen_get_cell(t->screen, (VTermPos){ y, x }, &c);
            if (c.chars[0] == 0 || c.chars[0] == (uint32_t)-1) continue; // empty, or the right half of a wide char

            s.fg = color(t->screen, c.fg);
            s.bg = color(t->screen, c.bg);
            s.attr = (c.attrs.bold ? VUI_ATTR_BOLD : 0) | (c.attrs.underline ? VUI_ATTR_UNDERLINE : 0) | (c.attrs.reverse ? VUI_ATTR_REVERSE : 0);
            // shortcut: only the base character is drawn, combining marks are dropped
            scr_set(scr, r.x + x, r.y + y, c.chars[0], s);
        }
    }

    vterm_state_get_cursorpos(vterm_obtain_state(t->vt), &cur);

    if (node->focused && t->cursor_visible && (unsigned)cur.col < r.width && (unsigned)cur.row < r.height) {
        VTermScreenCell c;
        VuiStyle s = { .attr = VUI_ATTR_REVERSE };

        vterm_screen_get_cell(t->screen, cur, &c);
        scr_set(scr, r.x + cur.col, r.y + cur.row, c.chars[0] && c.chars[0] != (uint32_t)-1 ? c.chars[0] : ' ', s);
    }
}

static int term_event(VuiNode *node, VuiCtx *ui, const VuiEvent *e)
{
    VuiTerminal *t = (VuiTerminal *)node;
    VTermModifier mod = VTERM_MOD_NONE;

    (void)ui;

    if (t->fd < 0) return 0;

    switch (e->type) {
    case VUI_EVENT_PASTE_START: vterm_keyboard_start_paste(t->vt); return 1;
    case VUI_EVENT_PASTE_END:   vterm_keyboard_end_paste(t->vt); return 1;
    case VUI_EVENT_TEXT:        vterm_keyboard_unichar(t->vt, e->text.codepoint, VTERM_MOD_NONE); return 1;
    case VUI_EVENT_KEY:         break;
    default:                    return 0; // shortcut: mouse and wheel aren't forwarded, and there is no scrollback
    }

    if (e->key.modifiers & VUI_MOD_SHIFT) mod |= VTERM_MOD_SHIFT;
    if (e->key.modifiers & VUI_MOD_ALT)   mod |= VTERM_MOD_ALT;
    if (e->key.modifiers & VUI_MOD_CTRL)  mod |= VTERM_MOD_CTRL;

    if (e->key.key == VUI_KEY_CHAR) {
        vterm_keyboard_unichar(t->vt, e->key.character, mod);
        return 1;
    }

    if (e->key.key >= VUI_KEY_F1 && e->key.key <= VUI_KEY_F12) {
        vterm_keyboard_key(t->vt, VTERM_KEY_FUNCTION(e->key.key - VUI_KEY_F1 + 1), mod);
        return 1;
    }

    static const struct { VuiKey from; VTermKey to; } map[] = {
        { VUI_KEY_ENTER, VTERM_KEY_ENTER }, { VUI_KEY_BACKSPACE, VTERM_KEY_BACKSPACE }, { VUI_KEY_TAB, VTERM_KEY_TAB },
        { VUI_KEY_ESCAPE, VTERM_KEY_ESCAPE }, { VUI_KEY_DELETE, VTERM_KEY_DEL }, { VUI_KEY_UP, VTERM_KEY_UP },
        { VUI_KEY_DOWN, VTERM_KEY_DOWN }, { VUI_KEY_LEFT, VTERM_KEY_LEFT }, { VUI_KEY_RIGHT, VTERM_KEY_RIGHT },
        { VUI_KEY_HOME, VTERM_KEY_HOME }, { VUI_KEY_END, VTERM_KEY_END }, { VUI_KEY_PAGE_UP, VTERM_KEY_PAGEUP },
        { VUI_KEY_PAGE_DOWN, VTERM_KEY_PAGEDOWN }, { VUI_KEY_INSERT, VTERM_KEY_INS },
    };

    // Shift-Tab is left to the focus system (F6 also moves out), like the text area
    if (e->key.key == VUI_KEY_TAB && (mod & VTERM_MOD_SHIFT)) return 0;

    for (unsigned int i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (map[i].from == e->key.key) {
            vterm_keyboard_key(t->vt, map[i].to, mod);
            return 1;
        }
    }

    return 0;
}

// libvterm -> child: keyboard input and replies to queries (cursor position, ...)
static void to_child(const char *s, size_t len, void *user)
{
    VuiTerminal *t = user;

    while (t->fd >= 0 && len > 0) {
        ssize_t n = write(t->fd, s, len);

        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break; // shortcut: a full pty buffer drops the input, when a child stops reading
        s += n;
        len -= n;
    }
}

static int set_prop(VTermProp prop, VTermValue *val, void *user)
{
    VuiTerminal *t = user;

    if (prop == VTERM_PROP_CURSORVISIBLE) t->cursor_visible = val->boolean;
    return 1;
}

static const VTermScreenCallbacks screen_cb = { .settermprop = set_prop };

static void child_gone(VuiTerminal *t)
{
    int status = 0;

    vui_unwatch_fd(vui_ctx(), t->fd);
    close(t->fd);
    t->fd = -1;
    waitpid(t->pid, &status, 0);
    t->pid = 0;
    if (t->on_exit != NULL) t->on_exit(&t->node, status);
}

static void from_child(VuiCtx *ui, int fd, void *user)
{
    VuiTerminal *t = user;
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf));

    (void)ui;

    if (n > 0) {
        vterm_input_write(t->vt, buf, n);
        vterm_screen_flush_damage(t->screen);
    } else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
        child_gone(t);
    }
}

static void term_destroy(VuiNode *node)
{
    VuiTerminal *t = (VuiTerminal *)node;

    if (t->fd >= 0) {
        vui_unwatch_fd(vui_ctx(), t->fd);
        close(t->fd);
        kill(t->pid, SIGHUP);
#ifdef __NuttX__
        // closing the master hangs the pty up: the shell sees EOF and exits. shortcut: not waited for
        // (a stuck child must not freeze the UI), so its exit status stays allocated until the app exits
        waitpid(t->pid, NULL, WNOHANG);
#else
        waitpid(t->pid, NULL, 0); // shortcut: a child ignoring SIGHUP blocks here, when that matters send SIGKILL after a timeout
#endif
    }

    vterm_free(t->vt);
}

VuiTerminal *terminal_create(const char *const argv[])
{
    VuiTerminal *t = (VuiTerminal *)node_alloc(sizeof(VuiTerminal));
    struct winsize ws = { .ws_row = 24, .ws_col = 80 };
    const char *sh[] = { getenv("SHELL") ? getenv("SHELL") : "/bin/sh", NULL };

    if (t == NULL) return NULL;
    if (argv == NULL) argv = sh;

    t->rows = 24;
    t->cols = 80;
    t->cursor_visible = 1;
    t->vt = vterm_new(t->rows, t->cols);

    if (t->vt == NULL) {
        node_destroy(&t->node);
        return NULL;
    }

    vterm_set_utf8(t->vt, 1);
    vterm_output_set_callback(t->vt, to_child, t);
    t->screen = vterm_obtain_screen(t->vt);
    vterm_screen_set_callbacks(t->screen, &screen_cb, t);
    vterm_screen_enable_altscreen(t->screen, 1);
    vterm_screen_reset(t->screen, 1);

    setenv("TERM", "xterm-256color", 1);

#ifdef __NuttX__
    {
        char slave[32];
        posix_spawn_file_actions_t fa;
        int ok = 0;

        t->fd = open("/dev/ptmx", O_RDWR | O_NOCTTY);

        if (t->fd >= 0 && unlockpt(t->fd) == 0 && ptsname_r(t->fd, slave, sizeof(slave)) == 0) {
            ioctl(t->fd, TIOCSWINSZ, (unsigned long)&ws);
            posix_spawn_file_actions_init(&fa);
            posix_spawn_file_actions_addopen(&fa, 0, slave, O_RDWR, 0);
            posix_spawn_file_actions_adddup2(&fa, 0, 1);
            posix_spawn_file_actions_adddup2(&fa, 0, 2);
            ok = posix_spawnp(&t->pid, argv[0], &fa, NULL, (char *const *)argv, NULL) == 0;
            posix_spawn_file_actions_destroy(&fa);
        }

        if (!ok) {
            if (t->fd >= 0) close(t->fd);
            t->fd = -1;
        }

        t->pid = ok ? t->pid : -1;
    }
#else
    t->pid = forkpty(&t->fd, NULL, NULL, &ws);

    if (t->pid == 0) {
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
#endif

    if (t->pid < 0) {
        t->fd = -1;
        t->node.destroy = term_destroy; // frees the vterm
        node_destroy(&t->node);
        return NULL;
    }

    t->node.destroy = term_destroy;
    t->node.draw = term_draw;
    t->node.on_event = term_event;
    t->node.focusable = 1;
    t->node.wants_tab = 1;
    t->node.grow = 1;
    vui_watch_fd(vui_ctx(), t->fd, from_child, t);
    return t;
}
