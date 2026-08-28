#include <stdint.h>

#ifndef VAPORUI_H
#define VAPORUI_H

typedef struct VuiCtx VuiCtx;
typedef uint32_t VuiCodepoint;

typedef struct {
    unsigned int width;
    unsigned int height;
} VuiSize;

typedef enum {
    VUI_EVENT_NONE,
    VUI_EVENT_KEY,
    VUI_EVENT_TEXT,
    VUI_EVENT_RESIZE
} VuiEventType;

typedef enum {
    VUI_MOD_NONE  = 0,
    VUI_MOD_SHIFT = 1 << 0,
    VUI_MOD_ALT   = 1 << 1,
    VUI_MOD_CTRL  = 1 << 2,
    VUI_MOD_META  = 1 << 3
} VuiModifiers;

typedef struct {
    VuiKey key;
    VuiModifiers modifiers;
} VuiKeyEvent;

typedef enum {
    VUI_KEY_NONE = 0,

    VUI_KEY_CTRL_C,

    VUI_KEY_ENTER,
    VUI_KEY_BACKSPACE,
    VUI_KEY_TAB,
    VUI_KEY_ESCAPE,

    VUI_KEY_UP,
    VUI_KEY_DOWN,
    VUI_KEY_LEFT,
    VUI_KEY_RIGHT,
} VuiKey;

typedef struct {
    VuiEventType type;

    union {
        struct {
            VuiKey key;
        } key;

        struct {
            VuiCodepoint codepoint;
        } text;

        VuiSize size;
    };
} VuiEvent;

VuiCtx*vui_init(void);
void vui_shutdown(VuiCtx *ui);

unsigned int vui_width(VuiCtx *ui);
unsigned int vui_height(VuiCtx *ui);

void vui_clear(VuiCtx *ui);

VuiEvent vui_poll_event(VuiCtx *ui);

#endif
