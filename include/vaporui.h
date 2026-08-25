#ifndef VAPORUI_H
#define VAPORUI_H

typedef struct VuiCtx VuiCtx;

typedef struct {
    unsigned int width;
    unsigned int height;
} VuiSize;

typedef enum {
    VUI_EVENT_NONE,
    VUI_EVENT_KEY,
    VUI_EVENT_RESIZE
} VuiEventType;

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

    VUI_KEY_CHAR
} VuiKey;

typedef struct {
    VuiEventType type;

    union {
        struct {
            VuiKey key;
            unsigned char character;
        } key;

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
