#ifndef VAPORUI_CTX_H
#define VAPORUI_CTX_H

#include "../include/vaporui.h"

#define VUI_MAX_WATCH 16
#define VUI_MAX_TIMERS 16

typedef struct { int fd; VuiFdCallback cb; void *userdata; } VuiWatch;
typedef struct { int id; uint64_t due; unsigned int interval; int repeat; VuiTimerCallback cb; void *userdata; } VuiTimer;

#define VUI_MAX_OVERLAYS 8

typedef struct { VuiNode *node; int flags; VuiNode *prev_focus; } VuiOverlay;

struct VuiCtx {
    VuiOverlay overlays[VUI_MAX_OVERLAYS]; // z-order, last = top
    unsigned int overlay_count;
    VuiNode *removed[VUI_MAX_OVERLAYS]; // unlinked, freed on the next render/dispatch
    unsigned int removed_count;
    int quit;
    int dirty;      // vui_next_event: something may have changed since the last frame
    uint64_t last_frame;
    int frame_wait; // vui_run: -1, or the most vui_poll_event may wait (a frame is due)
    VuiNode *root; // last root given to vui_render / vui_dispatch
    VuiWatch watches[VUI_MAX_WATCH];
    unsigned int watch_count;
    VuiTimer timers[VUI_MAX_TIMERS]; // id == 0: free slot
    int last_timer_id;

    VuiSize size;
    VuiScreen *screen;
    VuiNode *focus;
    VuiNode *capture; // gets mouse events until the button is released
    VuiNode *hover; // deepest node under the pointer; it and its ancestors have ->hovered set
};

// Set by vui_init() to a backend that forwards copies to the system clipboard (OSC 52).
void vui_overlays_free(VuiCtx *ui);
void vui_theme_reset(void);     // back to the default theme
void vui_clipboard_free(void);
int vui_platform_input_pending(void); // implemented by each backend
uint64_t vui_platform_now_ms(void);

extern void (*vui_clipboard_hook)(const char *utf8);

#endif
