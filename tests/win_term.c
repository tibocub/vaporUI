// Windows terminal widget (ConPTY) smoke test: runs `cmd /c echo hello`, checks the screen text and the exit code.
// `make winterm`, then run build/win-term.exe on Windows (Wine only checks the exit path).
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <io.h>
#include <windows.h>
#include "../include/vaporui.h"

extern unsigned int stub_ready_mask;
extern uint64_t stub_now;

static int exited = -1;
static void on_exit_cb(VuiNode *n, int status) { (void)n; exited = status; }

int main(void)
{
    VuiCtx *ui = vui_init();
    VuiScreen *scr = scr_create(40, 10);
    VuiTerminal *t = terminal_create((const char *const[]){ "cmd.exe", "/c", "echo", "hello world", "&", "exit", "3", NULL });
    char text[512];

    if (t == NULL) { puts("win_term: no ConPTY here (needs Windows 10 1809+)"); return 2; }
    t->on_exit = on_exit_cb;
    t->node.rect = (VuiRect){ 0, 0, 40, 10 };
    node_draw(&t->node, scr);

    for (int i = 0; i < 100 && exited < 0; i++) { // up to ~5 s
        Sleep(50);
        stub_now += 150; // the exit-poll timer must be due, or vui_next_event spins on the frozen clock
        DWORD avail = 0; // the stub reports ready when told: only tell it when there is something to read
        if (t->fd >= 0 && PeekNamedPipe((HANDLE)_get_osfhandle(t->fd), NULL, 0, NULL, &avail, NULL) && avail > 0) stub_ready_mask = 1;
        vui_next_event(ui, &t->node);
        stub_ready_mask = 0;
    }

    assert(exited >= 0);
    assert((exited >> 8) == 3);

    // select the first rows with the mouse and read the text back
    VuiEvent e = { .type = VUI_EVENT_MOUSE, .mouse = { VUI_MOUSE_LEFT, VUI_MOUSE_PRESS, 0, 0, 0 } };
    t->node.on_event(&t->node, ui, &e);
    e.mouse.action = VUI_MOUSE_MOVE; e.mouse.x = 39; e.mouse.y = 9;
    t->node.on_event(&t->node, ui, &e);
    terminal_selection_text(t, text, sizeof text);
    printf("[%s]\n", text);
    if (strstr(text, "hello world") == NULL) {
        // Wine's ConPTY writes the program's screen to its own stdout instead of our pipe: only the exit path is checkable there
        assert(GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != NULL);
        puts("win_term: under Wine, output not checked");
    }

    node_destroy(&t->node);
    puts("win_term: ok");
    return 0;
}
