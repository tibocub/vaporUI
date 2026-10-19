#include <stddef.h>
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
    VUI_EVENT_RESIZE,
    VUI_EVENT_MOUSE,
    VUI_EVENT_PASTE_START, // pasted content follows as VUI_EVENT_TEXT (newlines as '\n')
    VUI_EVENT_PASTE_END
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

typedef enum {
    VUI_MOUSE_NONE, // motion with no button held
    VUI_MOUSE_LEFT,
    VUI_MOUSE_MIDDLE,
    VUI_MOUSE_RIGHT,
    VUI_MOUSE_WHEEL_UP,
    VUI_MOUSE_WHEEL_DOWN,
    VUI_MOUSE_WHEEL_LEFT,
    VUI_MOUSE_WHEEL_RIGHT
} VuiMouseButton;

typedef enum {
    VUI_MOUSE_PRESS,
    VUI_MOUSE_RELEASE,
    VUI_MOUSE_MOVE // motion, button says which one is held (if any)
} VuiMouseAction;

typedef struct {
    VuiMouseButton button;
    VuiMouseAction action;
    VuiModifiers modifiers;
    unsigned int x, y; // cell coordinates, 0-based
} VuiMouseEvent;

typedef struct {
    VuiEventType type;

    union {
        VuiKeyEvent key;
        VuiMouseEvent mouse;

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

// Opt-in: reporting mouse input disables the terminal's own text
// selection. level 1: clicks, drag, wheel. level 2: also plain motion,
// needed for node->hovered. 0 turns it off. Uses xterm SGR (1006) reporting, so anything that speaks
// that protocol (every modern terminal, or a vaporOS terminal / GPM
// shim that emits it) works without a backend change.
void vui_set_mouse(VuiCtx *ui, int level);

typedef struct {
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
} VuiRect;

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
void scr_fill(VuiScreen *scr, VuiRect rect, VuiCodepoint cp, VuiStyle style);

// Drawing outside the clip rect is ignored. node_draw() sets it to the
// node's rect for you; the previous clip is returned so it can be restored.
VuiRect scr_clip(VuiScreen *scr, VuiRect clip);
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

typedef enum {
    VUI_DIR_ROW,
    VUI_DIR_COLUMN,
} VuiDirection;

typedef struct VuiNode VuiNode;

// Called during node_draw() for any node that has one set. Drawing
// is clipped to node->rect.
typedef void (*VuiDraw)(VuiNode *node, VuiScreen *scr, void *userdata);

// Return 1 if the event was handled, 0 to let it bubble to the parent.
typedef int (*VuiOnEvent)(VuiNode *node, VuiCtx *ui, const VuiEvent *event);

struct VuiNode {
    VuiDirection direction;

    float grow;
    float shrink;
    unsigned int basis;

    unsigned int gap;
    unsigned int padding;

    VuiRect rect; // written by node_layout(), read by draw callbacks

    VuiDraw draw;
    VuiOnEvent on_event;
    void *userdata; // free for the app to use

    int focusable;  // reachable with Tab and by clicking
    int wants_tab;  // when focused, offer Tab to on_event() before moving focus
    int focused;    // set by vui_set_focus(), read it in draw callbacks
    int hovered;    // pointer is over this node or a descendant (needs vui_set_mouse(ui, 2))

    // internal -- use node_add()/node_remove(), don't touch directly
    VuiNode *parent;
    VuiNode **children;
    unsigned int child_count;
    unsigned int child_cap;
};

VuiNode *node_create(void);
// For custom nodes: embed VuiNode as the first member of your struct
// and allocate with node_alloc(sizeof(MyNode)); node_destroy() frees it.
VuiNode *node_alloc(size_t size);
void node_destroy(VuiNode *node); // frees the whole subtree

void node_add(VuiNode *parent, VuiNode *child);
void node_remove(VuiNode *child);

void node_layout(VuiNode *root, VuiRect available);
void node_draw(VuiNode *node, VuiScreen *scr);

// -- Focus and event routing --
//
// vui_render() lays out `root` over the whole terminal and draws it.
// vui_dispatch() routes one event: Tab/Shift-Tab move focus, a left
// click focuses the focusable node under the pointer, keys/text/paste go
// to the focused node, mouse events to the node under the pointer.
// Each is offered to on_event() up the parent chain until one returns 1.
// Returns 1 if some node handled it.

void vui_render(VuiCtx *ui, VuiNode *root);
int vui_dispatch(VuiCtx *ui, VuiNode *root, const VuiEvent *event);
void vui_set_focus(VuiCtx *ui, VuiNode *node);
VuiNode *vui_focus(VuiCtx *ui);

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

// -- Widgets --
//
// Each create function returns a node to node_add() somewhere. Widgets
// don't size themselves from their content yet: set basis/grow like on
// any node. Set node.userdata to reach your own state from callbacks.
// Strings passed in are not copied, keep them alive.

// Plain container that can paint a background.
typedef struct {
    VuiNode node;
    VuiStyle style;
} VuiDiv;

VuiDiv *div_create(VuiDirection direction);

// Draws a frame around its rect; children go inside it (padding 1).
typedef enum {
    VUI_BORDER_SINGLE,
    VUI_BORDER_ROUNDED,
    VUI_BORDER_DOUBLE
} VuiBorderStyle;

typedef struct {
    VuiNode node;
    const char *title; // may be NULL
    VuiBorderStyle border;
    VuiStyle style;
} VuiBorder;

VuiBorder *border_create(const char *title);

// Activated by Enter, Space or a left click.
typedef struct {
    VuiNode node;
    const char *label;
    void (*on_click)(VuiNode *button);
} VuiButton;

VuiButton *button_create(const char *label, void (*on_click)(VuiNode *button));

// A list of items navigated with Up/Down/Home/End or the mouse, toggled
// with Space/Enter/click. In radio mode exactly one item is selected.
typedef struct {
    VuiNode node;
    const char **items;
    unsigned int count;
    unsigned int cursor;
    unsigned int top;  // first visible row
    unsigned int hover_row; // item under the pointer, when node.hovered
    int radio;
    void (*on_change)(VuiNode *list, unsigned int index);
    unsigned char checked[]; // one flag per item
} VuiChecklist;

VuiChecklist *checklist_create(const char **items, unsigned int count, int radio);

#endif
