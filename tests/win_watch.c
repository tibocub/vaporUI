// Windows only: a watched pipe wakes the loop and a hangup keeps firing until unwatched.
// Run under a console (or wine): build/win-watch.exe prints "win-watch: ok".
#include <io.h>
#include <stdio.h>
#include <fcntl.h>

#include "../include/vaporui.h"

static int fds[2], got, ticks;

static void on_tick(VuiCtx *ui, void *ud)
{
    (void)ui; (void)ud;
    if (++ticks == 1) _write(fds[1], "x", 1);
    if (ticks == 2) _close(fds[1]);
}

static void on_read(VuiCtx *ui, int fd, void *ud)
{
    char c;

    (void)ud;
    if (_read(fd, &c, 1) == 1) got++;
    else { vui_unwatch_fd(ui, fd); vui_quit(ui); }
}

int main(void)
{
    VuiCtx *ui = vui_init();
    VuiNode *root = node_create();

    if (ui == NULL || _pipe(fds, 64, _O_BINARY) != 0) return 1;
    vui_watch_fd(ui, fds[0], on_read, NULL);
    vui_timer(ui, 100, 1, on_tick, NULL);
    vui_run(ui, root, NULL, NULL);
    node_destroy(root);
    vui_shutdown(ui);
    printf("win-watch: %s\n", got == 1 && ticks == 2 ? "ok" : "FAILED");
    return got == 1 && ticks == 2 ? 0 : 1;
}
