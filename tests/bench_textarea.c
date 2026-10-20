// Host timing of the text area on one very long line (the worst case for wrapping): build/bench-textarea
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../include/vaporui.h"
#include "../src/ctx.h"

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

int main(int argc, char **argv)
{
    unsigned int n = argc > 1 ? (unsigned int)atoi(argv[1]) : 8086;
    VuiCtx *ui = vui_init();
    VuiTextarea *ta = textarea_create(0);
    VuiNode *root = node_create();
    double t0 = now(), worst = 0;

    ta->node.grow = 1;
    node_add(root, &ta->node);
    vui_set_focus(ui, &ta->node);
    freopen("/dev/null", "w", stdout);

    for (unsigned int i = 0; i < n; i++) {
        double a = now();
        VuiEvent e = { .type = VUI_EVENT_TEXT, .text = { 'a' + i % 26 } };

        if (i % 7 == 6) e.text.codepoint = ' ';
        vui_dispatch(ui, root, &e);
        if (!getenv("PASTE") || i % 1000 == 999) vui_render(ui, root); // PASTE=1: frames are skipped while input is pending
        if (now() - a > worst) worst = now() - a;
    }

    fprintf(stderr, "%u chars typed one by one: total %.0f ms, worst key %.2f ms\n", n, (now() - t0) * 1000, worst * 1000);
    return 0;
}
