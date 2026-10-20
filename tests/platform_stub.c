// Stand-in backend so tests can build a VuiCtx (80x24) without a terminal.
#include "../include/vaporui.h"
#include "../src/ctx.h"

int vui_platform_init(void) { return 0; }
void vui_platform_shutdown(void) {}
int vui_platform_get_size(VuiSize *size) { *size = (VuiSize){ 80, 24 }; return 0; }
void vui_platform_clear(void) {}
void vui_platform_set_mouse(int level) { (void)level; }
uint64_t stub_now;           // tests move the clock
unsigned int stub_ready_mask; // bit i: watched fd i is readable
uint64_t vui_platform_now_ms(void) { return stub_now; }
int vui_platform_input_pending(void) { return 0; }
VuiEvent vui_platform_poll_event(int timeout_ms, const int *fds, unsigned int n, unsigned char *ready)
{
    (void)timeout_ms; (void)fds;
    for (unsigned int i = 0; i < n; i++) ready[i] = (stub_ready_mask >> i) & 1;
    return (VuiEvent){ .type = VUI_EVENT_NONE };
}
