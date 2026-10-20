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
    VUI_EVENT_PASTE_END,
    VUI_EVENT_WAKE, // a watched fd or timer callback ran: redraw if the state it changed is shown
    VUI_EVENT_CLOSE // the terminal went away (EOF / hangup): vui_run() quits on it
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
// The ui made by vui_init(), for callbacks that have no ui argument (e.g.
// a button's on_click opening a window). There is one terminal, so one ui.
VuiCtx *vui_ctx(void);
void vui_shutdown(VuiCtx *ui);

unsigned int vui_width(VuiCtx *ui);
unsigned int vui_height(VuiCtx *ui);

void vui_clear(VuiCtx *ui);
void vui_flush(VuiCtx *ui);

// Blocks until there is input, a resize, or a watched fd / timer fires
// (VUI_EVENT_WAKE, after its callback ran). May also return VUI_EVENT_NONE
// (e.g. half an escape sequence arrived): just poll again.
VuiEvent vui_poll_event(VuiCtx *ui);

// -- Event loop --
//
// Extra sources for vui_poll_event(): file descriptors (a pty or pipe of a
// child process, a socket) and timers. Callbacks run on the polling thread
// and may add/remove watches and timers, including their own. Up to 16 of
// each.

typedef void (*VuiFdCallback)(VuiCtx *ui, int fd, void *userdata);
typedef void (*VuiTimerCallback)(VuiCtx *ui, void *userdata);

// Calls cb whenever fd is readable (or hung up -- read() tells you which).
// Watching an fd again replaces its callback. Returns 0, or -1 if full.
int vui_watch_fd(VuiCtx *ui, int fd, VuiFdCallback cb, void *userdata);
void vui_unwatch_fd(VuiCtx *ui, int fd);

// Calls cb after `ms` milliseconds, every `ms` if `repeat`. Returns an id
// for vui_timer_cancel(), or 0 if full.
int vui_timer(VuiCtx *ui, unsigned int ms, int repeat, VuiTimerCallback cb, void *userdata);
void vui_timer_cancel(VuiCtx *ui, int id);

// Opt-in: reporting mouse input disables the terminal's own text
// selection. level 1: clicks, drag, wheel. level 2: also plain motion,
// needed for node->hovered. 0 turns it off. Uses xterm SGR (1006) reporting, so anything that speaks
// that protocol (every modern terminal, or a vaporOS terminal / GPM
// shim that emits it) works without a backend change.
void vui_set_mouse(VuiCtx *ui, int level);

// x/y are signed: scrolled content can sit above or left of the screen.
typedef struct {
    int x;
    int y;
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

// A color is one 32-bit value: 0 = the terminal's default, 1..16 = the ANSI
// colors, or VUI_COLOR256(n) / VUI_RGB(r, g, b). Colors the terminal can't
// show are approximated (see vui_set_color_depth).
typedef uint32_t VuiColor;

enum {
    VUI_COLOR_DEFAULT = 0,
    VUI_COLOR_BLACK,
    VUI_COLOR_RED,
    VUI_COLOR_GREEN,
    VUI_COLOR_YELLOW,
    VUI_COLOR_BLUE,
    VUI_COLOR_MAGENTA,
    VUI_COLOR_CYAN,
    VUI_COLOR_WHITE,
    VUI_COLOR_BRIGHT_BLACK, // = gray
    VUI_COLOR_BRIGHT_RED,
    VUI_COLOR_BRIGHT_GREEN,
    VUI_COLOR_BRIGHT_YELLOW,
    VUI_COLOR_BRIGHT_BLUE,
    VUI_COLOR_BRIGHT_MAGENTA,
    VUI_COLOR_BRIGHT_CYAN,
    VUI_COLOR_BRIGHT_WHITE,
};

#define VUI_COLOR256(n) (0x02000000u | ((n) & 0xff))
#define VUI_RGB(r, g, b) (0x01000000u | (((r) & 0xff) << 16) | (((g) & 0xff) << 8) | ((b) & 0xff))

// 4 = 16 colors, 8 = 256, 24 = truecolor. Detected from COLORTERM / TERM
// at init; call it to override (e.g. from a --colors flag).
void vui_set_color_depth(int depth);

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


typedef struct VuiScreen VuiScreen;

// -- Theme --
//
// The styles the built-in widgets draw with. vui_theme() returns the one
// shared theme: change fields to restyle every widget at once (e.g. call
// it once at startup). Zero-initialised VuiStyle fields mean "terminal
// default".
typedef struct {
    VuiStyle text;      // plain content
    VuiStyle focus;     // focused button, list cursor
    VuiStyle hover;     // pointer over a button / list row
    VuiStyle selection; // selected text
    VuiStyle border;    // frames
    VuiStyle title;     // window title bar
    VuiStyle title_off; // title bar of a window that isn't on top
    VuiStyle popup;     // popups, drop-downs, menus
    VuiStyle bar;       // menu / status bars
    VuiStyle scroll;    // scrollbar track
    VuiStyle thumb;     // scrollbar handle
} VuiTheme;

VuiTheme *vui_theme(void);

VuiScreen *vui_screen(VuiCtx *ui);

VuiScreen *scr_create(unsigned int width, unsigned int height);
void scr_destroy(VuiScreen *scr);
void scr_resize(VuiScreen *scr, unsigned int width, unsigned int height);

unsigned int scr_width(VuiScreen *scr);
unsigned int scr_height(VuiScreen *scr);

// Columns a character / string occupies: 0 (control, combining), 1, or 2 (CJK, emoji).
// scr_set / scr_puts handle wide characters: a wide cell takes two columns.
int vui_cp_width(VuiCodepoint cp);
unsigned int vui_str_width(const char *utf8);
unsigned int vui_strn_width(const char *utf8, size_t n); // first n bytes

void scr_set(VuiScreen *scr, unsigned int x, unsigned int y, VuiCodepoint cp, VuiStyle style);
void scr_puts(VuiScreen *scr, unsigned int x, unsigned int y, const char *utf8, VuiStyle style);
void scr_putn(VuiScreen *scr, unsigned int x, unsigned int y, const char *utf8, size_t n, VuiStyle style); // first n bytes
void scr_clear(VuiScreen *scr, VuiStyle style);
void scr_fill(VuiScreen *scr, VuiRect rect, VuiCodepoint cp, VuiStyle style);

// Drawing outside the clip rect is ignored. node_draw() sets it to the
// node's rect for you; the previous clip is returned so it can be restored.
VuiRect scr_clip(VuiScreen *scr, VuiRect clip);
void scr_flush(VuiScreen *scr);

// -- Element tree / layout --
//
// A flexbox subset: direction + basis/grow/shrink/gap along the main
// axis, per-side padding, `justify` (main axis) and `align` (cross axis)
// placement, content-based sizing (VUI_AUTO), and scrolling.
//
// Node is a plain struct, not opaque: set direction/grow/shrink/
// basis/gap/draw/userdata directly (padding through node_pad). Only
// child-list membership goes through node_add/node_remove, since those
// keep the children array and parent pointer consistent.

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

// basis value meaning "as big as my content": see node->measure
#define VUI_AUTO ((unsigned int)-1)

// What to do with the main-axis space the children leave unused.
typedef enum {
    VUI_JUSTIFY_START,
    VUI_JUSTIFY_CENTER,
    VUI_JUSTIFY_END,
    VUI_JUSTIFY_BETWEEN // first child at the start, last at the end, even gaps between
} VuiJustify;

// Where children sit on the cross axis. Anything but STRETCH sizes the
// child from its content (like VUI_AUTO).
typedef enum {
    VUI_ALIGN_STRETCH,
    VUI_ALIGN_START,
    VUI_ALIGN_CENTER,
    VUI_ALIGN_END
} VuiAlign;

struct VuiNode {
    VuiDirection direction;
    VuiJustify justify;
    VuiAlign align;

    float grow;
    float shrink;
    unsigned int basis; // size along the parent's main axis; VUI_AUTO sizes from content

    unsigned int gap;
    unsigned int pad_top, pad_right, pad_bottom, pad_left; // set with node_pad()

    // A scrolling container lets its children keep their natural size
    // along the main axis (no shrinking) and shifts them by `scroll`
    // cells; layout clamps it to [0, content - viewport]. `content` is the
    // main-axis size of everything inside, padding included.
    int scrolls;
    unsigned int scroll;
    unsigned int content;

    VuiRect rect; // written by node_layout(), read by draw callbacks

    VuiDraw draw;
    VuiOnEvent on_event;
    // Natural size of this node's content: its width if `row`, else its height.
    // Containers without one are measured from their children.
    unsigned int (*measure)(VuiNode *node, int row);
    void (*destroy)(VuiNode *node); // called by node_destroy, for widgets owning memory
    void *userdata; // free for the app to use

    int focusable;  // reachable with Tab and by clicking
    int wants_tab;  // when focused, offer Tab to on_event() before moving focus
    int focused;    // set by vui_set_focus(), read it in draw callbacks
    int hovered;    // pointer is over this node or a descendant (needs vui_set_mouse(ui, 2))
    int active;     // set by vui_render() on the topmost overlay

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

void node_pad(VuiNode *node, unsigned int top, unsigned int right, unsigned int bottom, unsigned int left);
void node_add(VuiNode *parent, VuiNode *child);
void node_remove(VuiNode *child);

void node_layout(VuiNode *root, VuiRect available);
unsigned int node_measure(VuiNode *node, int row); // width if row, else height; honours basis and children
void node_draw(VuiNode *node, VuiScreen *scr);

// -- Clipboard --
//
// An in-process clipboard shared by all widgets, also forwarded to the
// system clipboard through the terminal (OSC 52) where supported. There
// is no portable way to read the system clipboard from a terminal: use
// the terminal's own paste (Ctrl-Shift-V), which arrives as a paste event.

void vui_clipboard_set(const char *utf8);
const char *vui_clipboard_get(void); // never NULL

// -- Focus and event routing --
//
// vui_render() lays out `root` over the whole terminal and draws it.
// vui_dispatch() routes one event: Tab/Shift-Tab move focus, a left
// click focuses the focusable node under the pointer (and the node that
// handled a left press keeps getting the mouse until release, so drags work), keys/text/paste go
// to the focused node, mouse events to the node under the pointer.
// Each is offered to on_event() up the parent chain until one returns 1.
// Returns 1 if some node handled it.

// -- Overlays --
//
// Floating layers drawn over the root, in z-order, last = on top: windows,
// popups, drop-downs, dialogs. An overlay sits at `rect` (x, y, size; a 0
// width or height is measured from the content, and the rect is moved
// back on screen if it overflows) and can be moved afterwards by changing
// node->rect. The ui owns it: vui_overlay_remove() unlinks it right away
// and frees it before the next render/dispatch, so a node may remove its
// own overlay from inside its event handler.
//
// A left press raises the overlay under the pointer. Focus moves into a
// new overlay and goes back to where it was when it is removed.
//   VUI_OVERLAY_MODAL    only this overlay (and ones above it) get input
//   VUI_OVERLAY_DISMISS  Escape, or a click outside it, removes it

#define VUI_OVERLAY_MODAL   1
#define VUI_OVERLAY_DISMISS 2

// Returns 0, or -1 if there are too many overlays (8); the node stays yours then.
int vui_overlay_add(VuiCtx *ui, VuiNode *node, VuiRect rect, int flags);
void vui_overlay_remove(VuiCtx *ui, VuiNode *node); // node: the overlay's root, or any node inside it
void vui_overlay_raise(VuiCtx *ui, VuiNode *node);

void vui_render(VuiCtx *ui, VuiNode *root);
int vui_dispatch(VuiCtx *ui, VuiNode *root, const VuiEvent *event);
void vui_set_focus(VuiCtx *ui, VuiNode *node);
VuiNode *vui_focus(VuiCtx *ui);

// The whole loop: render (skipped while more input is already waiting), wait for an
// event, vui_dispatch() it, and offer it to `handler` only if no widget used it
// (NULL = none). Widgets get first refusal, so a text area keeps Ctrl-C for "copy" and
// lets it through to your handler (quit) when nothing is selected. To take a key
// before a widget does, wrap its on_event (see docs/widgets.md). Returns after
// vui_quit() or when the terminal closes. Write the loop yourself for more control.
typedef int (*VuiAppHandler)(VuiCtx *ui, const VuiEvent *event, void *userdata);

void vui_run(VuiCtx *ui, VuiNode *root, VuiAppHandler handler, void *userdata);
void vui_quit(VuiCtx *ui);

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
// size themselves from their content (VUI_AUTO basis) unless you set
// basis/grow. Set node.userdata to reach your own state from callbacks.
// Strings passed in are not copied, keep them alive.

// Static text, '\n' separates lines.
typedef struct {
    VuiNode node;
    const char *text;
    VuiStyle style;
} VuiLabel;

VuiLabel *label_create(const char *text);

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

// A vertical list of `count` rows whose text comes from `item()` when drawn,
// so the data can be huge, filtered, or generated -- nothing is copied.
// Up/Down/PageUp/PageDown/Home/End move the cursor, the wheel scrolls,
// Enter or a click calls on_select. After changing what item() returns,
// call list_set_count().
typedef struct VuiList VuiList;

struct VuiList {
    VuiNode node;
    unsigned int count;
    unsigned int cursor;
    unsigned int top;       // first visible row
    unsigned int hover_row; // row under the pointer, when node.hovered
    int follow;             // scroll to the cursor on next draw
    const char *(*item)(VuiList *list, unsigned int index); // valid until the next call
    void (*on_select)(VuiList *list, unsigned int index);
    void (*on_cursor)(VuiList *list, unsigned int index);   // the cursor moved (keys, click)
};

VuiList *list_create(unsigned int count, const char *(*item)(VuiList *list, unsigned int index));
void list_set_count(VuiList *list, unsigned int count); // clamps cursor and scroll
void list_set_cursor(VuiList *list, unsigned int index);

// Content scrolls inside it: put children in it and give it a size (it grows
// by default). The wheel, arrows, PageUp/Down, Home/End and the scrollbar
// (right edge, clickable and draggable) scroll it; focusing a child scrolls
// it into view. Children keep their natural size and don't shrink.
VuiNode *scroll_create(void);

// Movable, resizable frame with a title bar and a close box, for use as an
// overlay (see vui_overlay_add()): drag the title bar to move it, the
// bottom-right corner to resize it, [x] to close it (on_close, or by
// default it removes itself). Children go inside, like a border.
typedef struct {
    VuiNode node;
    const char *title;
    void (*on_close)(VuiNode *window);
    int drag, grab_x, grab_y; // internal
} VuiWindow;

VuiWindow *window_create(const char *title);

// A one-line selector: Enter, Space, Down or a click opens a popup list of
// the items; picking one sets `selected` and calls on_change.
typedef struct {
    VuiNode node;
    const char **items;
    unsigned int count;
    unsigned int selected;
    void (*on_change)(VuiNode *dropdown, unsigned int index);
} VuiDropdown;

VuiDropdown *dropdown_create(const char **items, unsigned int count, void (*on_change)(VuiNode *dropdown, unsigned int index));

// A context menu: a popup list of `items` at (x, y) (clamped on screen); picking one
// closes it and calls on_pick(ud, index). Escape or a click outside closes it.
void menu_popup(int x, int y, const char **items, unsigned int count, void (*on_pick)(void *ud, unsigned int index), void *ud);

// A menu bar: put it in a bar. Click a title (or focus it, Left/Right, Enter/Down) to drop
// down its items; Left/Right in the open menu switch to the neighbour. A pick calls
// on_pick(bar, menu index, item index). The menus array must outlive the bar.
typedef struct {
    const char *title;
    const char **items;
    unsigned int count;
} VuiMenu;

typedef struct VuiMenubar {
    VuiNode node;
    const VuiMenu *menus;
    unsigned int count, active;
    void (*on_pick)(struct VuiMenubar *bar, unsigned int menu, unsigned int item);
} VuiMenubar;

VuiMenubar *menubar_create(const VuiMenu *menus, unsigned int count, void (*on_pick)(VuiMenubar *bar, unsigned int menu, unsigned int item));

// A one-line row in the theme's bar style, e.g. a status or menu bar.
// Put labels in it (set their style to vui_theme()->bar so they match).
VuiDiv *bar_create(void);

// Single-line text field with a fixed capacity (in characters). Handles
// typing, paste, Backspace/Delete, Left/Right/Home/End, Ctrl-A/E (line
// ends), Ctrl/Alt+Left/Right (words), Ctrl-W / Ctrl-Backspace (delete
// word back), Ctrl-U / Ctrl-K (delete to start / end), and click to
// place the cursor. Enter calls on_submit. Scrolls sideways when the
// text is wider than the node.
typedef struct {
    VuiNode node;
    unsigned int len, cap;
    unsigned int cursor;
    unsigned int left; // first visible character
    void (*on_change)(VuiNode *input);
    void (*on_submit)(VuiNode *input);
    VuiCodepoint text[]; // cap entries
} VuiInput;

VuiInput *input_create(unsigned int max_chars);
void input_set_text(VuiInput *input, const char *utf8);
// Copies the text as NUL-terminated UTF-8, truncated to fit; returns its length in bytes.
size_t input_get_text(const VuiInput *input, char *out, size_t size);

// Multi-line text editor. The text is one buffer of characters with '\n'
// between lines; it grows as needed up to max_chars (0 = unlimited).
//
// Keys: arrows (Up/Down remember the column), Home/End (visual row),
// Ctrl-Home/End, PageUp/PageDown, Ctrl/Alt+Left/Right (words); Shift +
// any of those selects, and so does dragging the mouse. Enter, Tab
// (spaces up to the next multiple of tab_width), Backspace/Delete,
// Ctrl-W / Ctrl-Backspace, Ctrl-U (to line start), Ctrl-K (to line end,
// or joins the next line), Ctrl-Z undo, Ctrl-Y / Ctrl-Shift-Z redo.
// Clipboard: Ctrl-C copy (only with a selection, so Ctrl-C elsewhere
// still bubbles), Ctrl-X / Shift-Delete cut, Ctrl-V / Shift-Insert
// paste, Ctrl-Insert copy. Alt-A selects all. Wheel scrolls, click
// places the cursor, paste events insert text.
//
// Lines wrap at word boundaries when `wrap` is set (default); otherwise
// they scroll sideways. Shift-Tab and Ctrl-Tab leave the field.
typedef struct VuiHistory VuiHistory;

typedef struct {
    VuiNode node;
    VuiCodepoint *text;
    unsigned int len, cap, max;
    unsigned int cursor; // index into text
    unsigned int anchor; // other end of the selection; equals cursor when nothing is selected
    unsigned int want;   // column Up/Down try to return to ((unsigned)-1 = not worked out yet)
    unsigned int top;    // index of the first visible row's start
    unsigned int left;   // first visible column (no-wrap mode)
    unsigned int tab_width;
    int wrap;            // default 1
    int auto_indent;     // Enter copies the current line's indentation; default 0
    int reveal;          // scroll to the cursor on next draw
    unsigned int wrap_key; // internal: width / wrap the current `top` was validated for
    int changed;
    VuiHistory *history;
    void (*on_change)(VuiNode *textarea);
} VuiTextarea;

VuiTextarea *textarea_create(unsigned int max_chars);
void textarea_set_text(VuiTextarea *ta, const char *utf8);
size_t textarea_get_text(const VuiTextarea *ta, char *out, size_t size);
void textarea_cursor_pos(const VuiTextarea *ta, unsigned int *line, unsigned int *column); // 0-based, column in characters
void textarea_select_all(VuiTextarea *ta);
size_t textarea_get_selection(const VuiTextarea *ta, char *out, size_t size); // empty string if none
void textarea_undo(VuiTextarea *ta);
void textarea_redo(VuiTextarea *ta);

#endif
