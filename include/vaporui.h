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

    VUI_KEY_CHAR, // a letter/digit + modifiers, e.g. Ctrl-D; see VuiKeyEvent.character

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
    VUI_KEY_INSERT,

    VUI_KEY_F1, // F2..F12 follow contiguously: VUI_KEY_F1 + n - 1
    VUI_KEY_F2, VUI_KEY_F3, VUI_KEY_F4, VUI_KEY_F5, VUI_KEY_F6,
    VUI_KEY_F7, VUI_KEY_F8, VUI_KEY_F9, VUI_KEY_F10, VUI_KEY_F11, VUI_KEY_F12,
} VuiKey;

typedef struct {
    VuiKey key;
    VuiModifiers modifiers;
    VuiCodepoint character; // valid when key == VUI_KEY_CHAR
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

// -- Element tree / layout --
//
// A flexbox subset: direction + basis/grow/shrink/gap along the main
// axis, uniform padding, and cross-axis stretch (the only cross-axis
// behavior for now -- no align-items variants yet). No content-based
// sizing either -- basis is whatever the app sets, since there's no
// text-measurement system yet to size a node from its content.
//
// Node is a plain struct, not opaque: set direction/grow/shrink/
// basis/gap/padding/draw/userdata directly. Only child-list membership
// goes through node_add/node_remove, since those keep the children
// array and parent pointer consistent.

typedef struct {
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
} VuiRect;

typedef enum {
    VUI_DIR_ROW,
    VUI_DIR_COLUMN,
} VuiDirection;

typedef struct VuiNode VuiNode;

// Called during node_draw() for any node that has one set. Draw
// only within node->rect -- nothing clips for you yet.
typedef void (*VuiDraw)(VuiNode *node, VuiScreen *scr, void *userdata);

struct VuiNode {
    VuiDirection direction;

    float grow;
    float shrink;
    unsigned int basis;

    unsigned int gap;
    unsigned int padding;

    VuiRect rect; // written by node_layout(), read by draw callbacks

    VuiDraw draw;
    void *userdata;

    // internal -- use node_add()/node_remove(), don't touch directly
    VuiNode *parent;
    VuiNode **children;
    unsigned int child_count;
    unsigned int child_cap;
};

VuiNode *node_create(void);
void node_destroy(VuiNode *node); // frees the whole subtree

void node_add(VuiNode *parent, VuiNode *child);
void node_remove(VuiNode *child);

void node_layout(VuiNode *root, VuiRect available);
void node_draw(VuiNode *node, VuiScreen *scr);

// -- Controls --
//
// A flat key -> action binding table. Doesn't know about the tree or
// focus -- that's the app's job for now: keep one global VuiControls
// checked first (unconditional, e.g. Ctrl-C to quit) and dispatch to
// a more specific one only if global didn't handle it. Once nodes can
// be focused, a node-owned VuiControls slots into the same dispatch
// call, no change needed here.

typedef void (*VuiAction)(VuiCtx *ui, const VuiKeyEvent *event, void *userdata);

typedef struct VuiControls VuiControls;

VuiControls *ctl_create(void);
void ctl_destroy(VuiControls *c);

// Binding the same key again replaces the previous action instead of
// shadowing it silently.
void ctl_bind(
    VuiControls *c,
    VuiKey key, VuiModifiers modifiers, VuiCodepoint character,
    VuiAction action, void *userdata
);

// Returns 1 and fires the action if this event matches a binding, 0
// otherwise.
int ctl_dispatch(VuiControls *c, VuiCtx *ui, const VuiKeyEvent *event);

#endif
