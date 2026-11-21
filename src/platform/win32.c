// Windows backend. Output goes through the console's VT processing (the same escape sequences as on POSIX), but
// input is read natively: KEY_EVENT records carry the virtual key, the scan code (the physical key) and the
// modifier state, MOUSE_EVENT records the mouse. src/winkeys.c turns them into events (it is pure and tested);
// no escape sequences are decoded here and nothing depends on what a terminal chooses to send.
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "../../include/vaporui.h"
#include "../winkeys.h"
#include "../scancode.h"
#include "../input.h"
#include "../ctx.h"
#define VTOUT_NO_PASTE
#define VUI_OUT_BUFFER 16384 // one console write per few frames beats a write per cell
#include "vtout.h"

#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef DISABLE_NEWLINE_AUTO_RETURN
#define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif

static HANDLE con_in = INVALID_HANDLE_VALUE, con_out = INVALID_HANDLE_VALUE;
static DWORD saved_in_mode, saved_out_mode;
static UINT saved_cp;
static int saved_stdout_fd = -1; // the program's real stdout while the UI draws on the console
// The active keyboard layout of the foreground window (0x040C is French France; other French layouts differ)
static const char *detect_layout(void)
{
    DWORD thread = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    unsigned int lang = (unsigned int)(UINT_PTR)GetKeyboardLayout(thread) & 0xFFFFu;

    return lang == 0x040C ? "fr" : lang == 0x0409 ? "us" : NULL;
}

static int mouse_level;
static unsigned int prev_buttons;
static WCHAR high_surrogate;

// Console input mode: no line editing, echo or Ctrl-C handling (Ctrl-C arrives as a key). Quick-edit (selecting
// text with the mouse) stays on until the app asks for mouse input.
static DWORD input_mode(void)
{
    return mouse_level > 0 ? (ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT)
                           : (ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_QUICK_EDIT_MODE);
}

int vui_platform_init(void)
{
    DWORD in_mode, out_mode;

    // CONIN$ / CONOUT$ are the console even when stdin/stdout are redirected (`dir | picker`)
    con_in = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    con_out = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (con_in == INVALID_HANDLE_VALUE || con_out == INVALID_HANDLE_VALUE
        || !GetConsoleMode(con_in, &saved_in_mode) || !GetConsoleMode(con_out, &saved_out_mode)) {
        return -1;
    }

    in_mode = input_mode();
    out_mode = ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;

    if (!SetConsoleMode(con_in, in_mode) || !SetConsoleMode(con_out, out_mode)) {
        SetConsoleMode(con_in, saved_in_mode);
        return -1;
    }

    saved_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);

    // The UI is drawn on the console through stdio; the program's own stdout comes back at shutdown
    fflush(stdout);
    saved_stdout_fd = _dup(1);

    if (saved_stdout_fd < 0 || freopen("CONOUT$", "wb", stdout) == NULL) {
        SetConsoleMode(con_in, saved_in_mode);
        SetConsoleMode(con_out, saved_out_mode);
        SetConsoleOutputCP(saved_cp);
        return -1;
    }

    vui_layout_hook = detect_layout;
    mouse_level = 0;
    prev_buttons = 0;
    high_surrogate = 0;
    vui_clipboard_hook = vtout_osc52_copy;
    vtout_start();

    return 0;
}

void vui_platform_shutdown(void)
{
    vtout_stop();
    SetConsoleMode(con_in, saved_in_mode);
    SetConsoleMode(con_out, saved_out_mode);
    SetConsoleOutputCP(saved_cp);
    fflush(stdout);
    _dup2(saved_stdout_fd, 1);
    _close(saved_stdout_fd);
    _setmode(1, _O_TEXT);
    CloseHandle(con_in);
    CloseHandle(con_out);
}

void vui_platform_set_mouse(int level)
{
    mouse_level = level;
    prev_buttons = 0;
    SetConsoleMode(con_in, input_mode());
}

int vui_platform_get_size(VuiSize *size)
{
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (size == NULL || !GetConsoleScreenBufferInfo(con_out, &info)) {
        return -1;
    }

    size->width = info.srWindow.Right - info.srWindow.Left + 1;
    size->height = info.srWindow.Bottom - info.srWindow.Top + 1;

    return 0;
}

void vui_platform_clear(void)
{
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
}

uint64_t vui_platform_now_ms(void)
{
    return GetTickCount64();
}

int vui_platform_input_pending(void)
{
    return WaitForSingleObject(con_in, 0) == WAIT_OBJECT_0;
}

// A watched fd is a CRT fd (_pipe, _open): pipes are readable when they hold data or are
// broken (hangup); other kinds report ready. Sockets (not CRT fds) can't be watched.
static int fd_ready(int fd)
{
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    DWORD avail;

    if (h == INVALID_HANDLE_VALUE || GetFileType(h) != FILE_TYPE_PIPE) {
        return 1;
    }

    return !PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL) || avail > 0;
}

// The key typing nothing but itself on the foreground window's layout: ToUnicodeEx with an empty key state (and
// the "don't touch the keyboard state" flag, so a dead key stays pending). 0 for none or a dead key.
static uint32_t unshifted_char(unsigned vk, unsigned scan)
{
    BYTE keys[256] = { 0 };
    WCHAR buf[4];
    DWORD thread = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    int n = ToUnicodeEx(vk, scan, keys, buf, 4, 0x4, GetKeyboardLayout(thread));

    return n == 1 ? buf[0] : 0;
}

static unsigned char vk_down[256]; // keys seen going down, to tell auto-repeat from a fresh press

static VuiEvent key_record(const KEY_EVENT_RECORD *k)
{
    VuiEvent event = { .type = VUI_EVENT_NONE };
    uint32_t uc = k->uChar.UnicodeChar;
    unsigned vk = k->wVirtualKeyCode;
    int repeat = vk < 256 && vk_down[vk];

    if (vk < 256) vk_down[vk] = k->bKeyDown != 0;

    if (!k->bKeyDown) {
        // releases only for apps that asked (vui_set_key_release); a held key repeats as separate records
        if (!vui_key_release_reports || !vui_winkey_state(vk, k->wVirtualScanCode, 0, unshifted_char(vk, k->wVirtualScanCode),
                                                          k->dwControlKeyState, &event)) {
            event.type = VUI_EVENT_NONE;
        }

        return event;
    }

    if (uc >= 0xD800 && uc < 0xDC00) { // half of a surrogate pair: wait for the other
        high_surrogate = (WCHAR)uc;
        return event;
    }

    if (uc >= 0xDC00 && uc < 0xE000) {
        uc = high_surrogate == 0 ? 0 : 0x10000 + (((uint32_t)high_surrogate - 0xD800) << 10) + (uc - 0xDC00);
    }

    high_surrogate = 0;

    if (!vui_winkey_translate(k->wVirtualKeyCode, k->wVirtualScanCode, uc, unshifted_char(k->wVirtualKeyCode, k->wVirtualScanCode),
                              k->dwControlKeyState, &event)) {
        event.type = VUI_EVENT_NONE;
        if (vui_key_release_reports && vui_winkey_state(vk, k->wVirtualScanCode, 1, 0, k->dwControlKeyState, &event)) event.key.repeat = repeat;
        return event;
    }

    event.key.repeat = repeat;
    return event;
}

static VuiEvent mouse_record(const MOUSE_EVENT_RECORD *m)
{
    VuiEvent event = { .type = VUI_EVENT_NONE };
    CONSOLE_SCREEN_BUFFER_INFO info;
    int x, y;

    if (!GetConsoleScreenBufferInfo(con_out, &info)) {
        return event;
    }

    // positions are in the screen buffer; the app draws in the visible window
    x = m->dwMousePosition.X - info.srWindow.Left;
    y = m->dwMousePosition.Y - info.srWindow.Top;

    if (x < 0 || y < 0 || !vui_winmouse_translate(m->dwButtonState, prev_buttons, m->dwEventFlags, (unsigned)x, (unsigned)y,
                                                   m->dwControlKeyState, mouse_level, &event)) {
        event.type = VUI_EVENT_NONE;
    }

    prev_buttons = m->dwButtonState & 7u;
    return event;
}

// shortcut: with fds watched, the wait is sliced into 10 ms polls (the console handle and pipes
// can't share one wait); idle cost is ~100 wakeups/s. Upgrade to overlapped reads / worker threads.
VuiEvent vui_platform_poll_event(int timeout_ms, const int *fds, unsigned int n, unsigned char *ready)
{
    VuiEvent event = { .type = VUI_EVENT_NONE };
    INPUT_RECORD rec;
    DWORD got;
    uint64_t end = vui_platform_now_ms() + (timeout_ms < 0 ? 0 : (unsigned int)timeout_ms);

    for (;;) {
        DWORD wait = timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms;
        int any = 0;

        for (unsigned int i = 0; i < n; i++) {
            ready[i] = (unsigned char)fd_ready(fds[i]);
            any |= ready[i];
        }

        if (any) {
            return event;
        }

        if (n > 0 && wait > 10) {
            wait = 10;
        }

        if (WaitForSingleObject(con_in, wait) == WAIT_OBJECT_0) {
            break;
        }

        if (n == 0 || (timeout_ms >= 0 && vui_platform_now_ms() >= end)) {
            return event;
        }
    }

    if (!ReadConsoleInputW(con_in, &rec, 1, &got) || got == 0) {
        return event;
    }

    switch (rec.EventType) {
    case WINDOW_BUFFER_SIZE_EVENT: {
        VuiSize size;

        if (vui_platform_get_size(&size) == 0) {
            event.type = VUI_EVENT_RESIZE;
            event.size = size;
        }

        return event;
    }

    case KEY_EVENT:   return key_record(&rec.Event.KeyEvent);
    case MOUSE_EVENT: return mouse_record(&rec.Event.MouseEvent);
    default:          return event;
    }
}
