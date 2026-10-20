// Windows backend: the console is switched to VT mode in both directions, so the same
// escape sequences and the same decoder as on POSIX apply; this file only handles raw
// console modes, size, resize events and waiting.
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <windows.h>

#include "../../include/vaporui.h"
#include "../input.h"
#include "../ctx.h"
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
static VuiInputDecoder input_decoder;

// UTF-8 bytes of the last key record not yet given to the decoder
static unsigned char pending[8];
static unsigned int pending_len, pending_pos;
static WCHAR high_surrogate;

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

    // No line editing, echo, or Ctrl-C handling (Ctrl-C arrives as a key); keys come as VT sequences
    in_mode = ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT;
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

    vui_input_reset(&input_decoder);
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
    vtout_set_mouse(level);
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

// UTF-16 unit -> UTF-8 in `pending` (a lone surrogate half waits for its pair)
static void queue_utf16(WCHAR w)
{
    unsigned long cp = w;

    if (w >= 0xD800 && w < 0xDC00) {
        high_surrogate = w;
        return;
    }

    if (w >= 0xDC00 && w < 0xE000) {
        if (high_surrogate == 0) return;
        cp = 0x10000 + (((unsigned long)high_surrogate - 0xD800) << 10) + (w - 0xDC00);
    }

    high_surrogate = 0;
    pending_len = pending_pos = 0;

    if (cp < 0x80) {
        pending[pending_len++] = (unsigned char)cp;
    } else if (cp < 0x800) {
        pending[pending_len++] = 0xC0 | (cp >> 6);
        pending[pending_len++] = 0x80 | (cp & 0x3F);
    } else if (cp < 0x10000) {
        pending[pending_len++] = 0xE0 | (cp >> 12);
        pending[pending_len++] = 0x80 | ((cp >> 6) & 0x3F);
        pending[pending_len++] = 0x80 | (cp & 0x3F);
    } else {
        pending[pending_len++] = 0xF0 | (cp >> 18);
        pending[pending_len++] = 0x80 | ((cp >> 12) & 0x3F);
        pending[pending_len++] = 0x80 | ((cp >> 6) & 0x3F);
        pending[pending_len++] = 0x80 | (cp & 0x3F);
    }
}

// shortcut: extra fds are not watched on Windows (consoles, pipes and sockets need different
// waits); timers still work. Upgrade when a Windows multiplexer needs child-process pipes.
VuiEvent vui_platform_poll_event(int timeout_ms, const int *fds, unsigned int n, unsigned char *ready)
{
    VuiEvent event = { .type = VUI_EVENT_NONE };
    INPUT_RECORD rec;
    DWORD got;
    int esc_first;

    (void)fds;

    for (unsigned int i = 0; i < n; i++) {
        ready[i] = 0;
    }

    if (pending_pos < pending_len) {
        return vui_input_feed(&input_decoder, pending[pending_pos++]);
    }

    esc_first = vui_input_pending_escape(&input_decoder) && (timeout_ms < 0 || VUI_ESCAPE_TIMEOUT_MS <= timeout_ms);

    if (esc_first) {
        timeout_ms = VUI_ESCAPE_TIMEOUT_MS;
    }

    if (WaitForSingleObject(con_in, timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms) != WAIT_OBJECT_0) {
        return esc_first ? vui_input_timeout(&input_decoder) : event;
    }

    if (!ReadConsoleInputW(con_in, &rec, 1, &got) || got == 0) {
        return event;
    }

    if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT) {
        VuiSize size;

        if (vui_platform_get_size(&size) == 0) {
            event.type = VUI_EVENT_RESIZE;
            event.size = size;
        }

        return event;
    }

    if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown && rec.Event.KeyEvent.uChar.UnicodeChar != 0) {
        queue_utf16(rec.Event.KeyEvent.uChar.UnicodeChar);

        if (pending_pos < pending_len) {
            return vui_input_feed(&input_decoder, pending[pending_pos++]);
        }
    }

    return event;
}
