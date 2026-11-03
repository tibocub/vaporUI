// WebAssembly backend (emscripten, built with -sASYNCIFY). There is no tty, no signal and no
// blocking poll() in a browser: the page owns the terminal (a web terminal such as wterm) and
// hands us the bytes the user typed through the Module.vui object (web/vui-glue.js); output goes
// out through stdout, which the page routes to the same terminal.
//
// Blocking: vui_platform_poll_event() must wait for input but the browser's main thread can't
// block. Asyncify rewrites the program so the wasm stack can be parked while a JS promise is
// pending (js_wait), then resumed. The C stays synchronous.
#include <emscripten.h>
#include <stdio.h>
#include <string.h>

#include "../../include/vaporui.h"
#include "../input.h"
#include "../ctx.h"
#include "vtout.h"

// js_wait: resolves 1 when input or a resize is pending, 0 on timeout, -1 when the page closed us.
EM_ASYNC_JS(int, js_wait, (int timeout_ms), { return await Module.vui.wait(timeout_ms); });
EM_JS(int, js_pending, (void), { return Module.vui.pending(); });                  // input bytes waiting
EM_JS(int, js_take, (unsigned char *buf, int n), { return Module.vui.take(HEAPU8.subarray(buf, buf + n)); });
EM_JS(int, js_resized, (void), { return Module.vui.resized(); });                  // true once per resize
EM_JS(int, js_cols, (void), { return Module.vui.cols(); });
EM_JS(int, js_rows, (void), { return Module.vui.rows(); });
EM_JS(double, js_now, (void), { return performance.now(); });

static VuiInputDecoder input_decoder;
static unsigned char rx[64];
static unsigned int rx_len, rx_pos;
static int resize_pending;

int vui_platform_init(void)
{
    vui_input_reset(&input_decoder);
    rx_len = rx_pos = 0;
    resize_pending = 0;
    vui_clipboard_hook = vtout_osc52_copy;
    vtout_start();
    return 0;
}

void vui_platform_shutdown(void)
{
    vtout_stop();
}

void vui_platform_set_mouse(int level)
{
    vtout_set_mouse(level);
}

int vui_platform_get_size(VuiSize *size)
{
    if (size == NULL) {
        return -1;
    }

    size->width = js_cols();
    size->height = js_rows();
    return size->width > 0 && size->height > 0 ? 0 : -1;
}

void vui_platform_clear(void)
{
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
}

uint64_t vui_platform_now_ms(void)
{
    return (uint64_t)js_now();
}

int vui_platform_input_pending(void)
{
    return resize_pending || rx_pos < rx_len || js_pending() > 0;
}

// Same contract as the POSIX backend. Watched fds don't exist here (no pipes, no children):
// the `fds` are ignored and never ready; timers still work, they're driven by timeout_ms.
VuiEvent vui_platform_poll_event(int timeout_ms, const int *fds, unsigned int n, unsigned char *ready)
{
    int esc = vui_input_pending_escape(&input_decoder), esc_first, got;
    VuiEvent event = { .type = VUI_EVENT_NONE };

    (void)fds;
    memset(ready, 0, n);

    if (resize_pending || js_resized()) {
        VuiSize size;

        resize_pending = 0;

        if (vui_platform_get_size(&size) == 0) {
            event.type = VUI_EVENT_RESIZE;
            event.size = size;
        }

        return event;
    }

    if (rx_pos < rx_len) {
        return vui_input_feed(&input_decoder, rx[rx_pos++]);
    }

    esc_first = esc && (timeout_ms < 0 || VUI_ESCAPE_TIMEOUT_MS <= timeout_ms);

    if (esc_first) {
        timeout_ms = VUI_ESCAPE_TIMEOUT_MS;
    }

    fflush(stdout);
    got = js_pending() > 0 ? 1 : js_wait(timeout_ms);

    if (got < 0) {
        event.type = VUI_EVENT_CLOSE;
        return event;
    }

    if (got == 0) {
        // Timed out: a pending escape sequence was a standalone ESC keypress.
        return esc_first ? vui_input_timeout(&input_decoder) : event;
    }

    if (js_resized()) {
        resize_pending = 1; // reported on the next call, after any input we read now
    }

    got = js_take(rx, sizeof(rx));

    if (got <= 0) {
        if (resize_pending) {
            resize_pending = 0;

            VuiSize size;

            if (vui_platform_get_size(&size) == 0) {
                event.type = VUI_EVENT_RESIZE;
                event.size = size;
            }
        }

        return event;
    }

    rx_len = (unsigned int)got;
    rx_pos = 0;
    return vui_input_feed(&input_decoder, rx[rx_pos++]);
}
