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

typedef enum {
    VUI_KEY_NONE = 0,

    VUI_KEY_CTRL_C,

    VUI_KEY_ENTER,
    VUI_KEY_BACKSPACE,
    VUI_KEY_TAB,
    VUI_KEY_ESCAPE,
    VUI_KEY_DELETE,

    VUI_KEY_UP,
    VUI_KEY_DOWN,
    VUI_KEY_LEFT,
    VUI_KEY_RIGHT,

    VUI_KEY_HOME,
    VUI_KEY_END,
    VUI_KEY_PAGE_UP,
    VUI_KEY_PAGE_DOWN,
} VuiKey;

typedef struct {
    VuiKey key;
    VuiModifiers modifiers;
} VuiKeyEvent;

typedef struct {
    VuiEventType type;

    union {
        VuiKeyEvent key;

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
void vui_flush(VuiCtx *ui);

VuiEvent vui_poll_event(VuiCtx *ui);

// -- Screen buffer --
//
// Double-buffered cell grid: draw into it with scr_set/scr_puts,
// nothing reaches the terminal until scr_flush(), which diffs
// against what's actually on screen and writes only what changed.
// vui_screen() returns the one owned by ui -- sized and resized
// automatically with the terminal, no separate lifecycle to manage.

typedef enum {
    VUI_COLOR_DEFAULT = 0,
    VUI_COLOR_BLACK,
    VUI_COLOR_RED,
    VUI_COLOR_GREEN,
    VUI_COLOR_YELLOW,
    VUI_COLOR_BLUE,
    VUI_COLOR_MAGENTA,
    VUI_COLOR_CYAN,
    VUI_COLOR_WHITE,
} VuiColor;

typedef enum {
    VUI_ATTR_NONE      = 0,
    VUI_ATTR_BOLD      = 1 << 0,
    VUI_ATTR_UNDERLINE = 1 << 1,
    VUI_ATTR_REVERSE   = 1 << 2,
} VuiAttr;

typedef struct {
    VuiColor fg;
    VuiColor bg;
    VuiAttr attr;
} VuiStyle;

typedef struct {
    VuiCodepoint codepoint;
    VuiStyle style;
} VuiCell;

typedef struct VuiScreen VuiScreen;

VuiScreen *vui_screen(VuiCtx *ui);

VuiScreen *scr_create(unsigned int width, unsigned int height);
void scr_destroy(VuiScreen *scr);
void scr_resize(VuiScreen *scr, unsigned int width, unsigned int height);

unsigned int scr_width(VuiScreen *scr);
unsigned int scr_height(VuiScreen *scr);

void scr_set(VuiScreen *scr, unsigned int x, unsigned int y, VuiCodepoint cp, VuiStyle style);
void scr_puts(VuiScreen *scr, unsigned int x, unsigned int y, const char *utf8, VuiStyle style);
void scr_clear(VuiScreen *scr, VuiStyle style);
void scr_flush(VuiScreen *scr);

#endif
