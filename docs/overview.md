# Overview

vaporUI is a small TUI library in C99: a cell buffer with diffed output, a flexbox-subset layout tree,
a decoder for terminal input, and a few widgets. No dependency beyond libc and a terminal.

## Anatomy of an app

```c
VuiCtx *ui = vui_init();
vui_set_mouse(ui, 2);                      // optional: 1 = clicks, 2 = clicks + hover

VuiNode *root = node_create();             // build a tree: node_add(parent, child)
node_add(root, &button_create("OK", on_ok)->node);

vui_set_focus(ui, first_focusable);
vui_run(ui, root, handler, NULL);          // render, wait, handler(), vui_dispatch, until vui_quit(ui)
// ...or write the loop yourself when you need control over it:
for (;;) {
    vui_render(ui, root);                  // layout over the whole terminal + draw + flush
    VuiEvent e;
    do e = vui_poll_event(ui); while (e.type == VUI_EVENT_NONE);   // blocks
    if (e.type == VUI_EVENT_KEY && ctl_dispatch(global, ui, &e.key)) continue;  // app-wide shortcuts
    vui_dispatch(ui, root, &e);            // focus, clicks, widget input
}
node_destroy(root);                        // frees the whole subtree
vui_shutdown(ui);                          // restores the terminal
```

The `examples/` directory is the reference for this; start with `buttons-and-lists.c`.

## Pipeline

```
bytes -> utf8.c / vt.c / input.c -> VuiEvent -> vui_dispatch (focus.c) -> node->on_event
tree  -> node_layout (node.c)    -> node->rect -> node_draw -> scr_set (screen.c, clipped) -> scr_flush -> terminal
```

| File | Role |
|------|------|
| `include/vaporui.h` | The whole public API |
| `src/vaporui.c`, `src/ctx.h` | Context (size, screen, focus, hover, overlays), init/shutdown, poll + fd watches + timers |
| `src/platform/posix.c` | Backend: termios raw mode, SIGWINCH, poll/read, terminal mode switches. The only platform-specific file |
| `src/utf8.c`, `src/vt.c`, `src/input.c` | Byte -> UTF-8 -> escape sequences -> events (see `input.md`) |
| `src/screen.c` | Front/back cell buffers, diff flush, clip rect, SGR output |
| `src/node.c` | Tree, flexbox layout, clipped draw |
| `src/focus.c` | Focus, Tab traversal, hit testing, hover, event bubbling, overlays, `vui_run` |
| `src/theme.c` | The shared theme (`vui_theme()`) the widgets draw with |
| `src/clipboard.c` | In-process clipboard + hook to the terminal's (OSC 52) |
| `src/controls.c` | Flat key -> action table for app-wide shortcuts |
| `src/widgets/*.c` | One file per widget |

## Layout in one paragraph

Every node has `direction` (row/column), and along its parent's main axis `basis`, `grow`, `shrink`;
plus `gap` and per-side padding (`node_pad(n, top, right, bottom, left)`) for its children.
`justify` places children along the main axis (start / center / end / between) and `align` across it
(stretch by default; start / center / end size the child from its content). `basis = VUI_AUTO`
sizes a node from its content (see `widgets.md`). A node with `scrolls = 1` lets its children keep
their size and shifts them by `scroll` (the `scroll_create` widget wraps this). `node->rect` is filled
by `node_layout` and is what draw callbacks and hit testing use; its x/y are signed because scrolled
content can sit above the screen.

## The event loop

`vui_poll_event` also waits on extra sources and runs their callbacks, then returns
`VUI_EVENT_WAKE` so you redraw:

- `vui_watch_fd(ui, fd, cb, userdata)` / `vui_unwatch_fd`: a pty or pipe of a child process, a socket. The callback
  fires while the fd is readable (or hung up: `read()` returns 0: unwatch it).
- `vui_timer(ui, ms, repeat, cb, userdata)` / `vui_timer_cancel`.
- Up to 16 of each. Callbacks run on the polling thread and may add or remove watches and timers.

`examples/event-loop.c` has both next to the keyboard.

## Overlays (windows, popups, dialogs)

`vui_overlay_add(ui, node, rect, flags)` puts a tree on top of the root, in z-order (last added =
topmost, at most 8): `rect` is where it sits (a 0 width/height means "measure it"); change `node->rect`
later to move it. The ui owns it from then on: `vui_overlay_remove(ui, any_node_inside)` unlinks it
immediately and frees it before the next render/dispatch, so a button can close its own dialog.
`vui_ctx()` returns the ui for callbacks that don't receive one.

- `VUI_OVERLAY_MODAL`: only this overlay gets input; Tab stays inside it.
- `VUI_OVERLAY_DISMISS`: Escape, or a click outside, removes it (the click is swallowed).
- Without flags it is a plain window: clicking raises it and moves focus into it, Tab visits the root then every overlay.
  Focus returns to where it was when an overlay closes. `node->active` is set on the topmost overlay (windows use it to light their title bar).
- `window_create`, `dropdown_create` build on this; `examples/windows.c` shows them.

## Theme

`vui_theme()` returns the one `VuiTheme` the built-in widgets draw with (text, focus, hover, selection, border, window
titles, popup, bar, scrollbar). Assign to its fields once at startup to restyle everything, including
the background: `text` is what the screen and containers are cleared with. Colors are 16 ANSI, `VUI_COLOR256(n)` or
`VUI_RGB(r, g, b)`; the ones the terminal can't show are approximated (`COLORTERM`/`TERM` pick the depth, `vui_set_color_depth` overrides).

## Events and focus

- `vui_dispatch` returns 1 if a node handled the event. Offered to `on_event` up the parent chain.
- Keys, text and paste go to the focused node. Mouse events go to the deepest node under the pointer
  (a left press also focuses the nearest focusable ancestor).
- Tab / Shift-Tab move focus through `focusable` nodes in tree order. A focused node with
  `wants_tab = 1` gets the Tab first (the text area does; set it to 0 and Tab navigates); if it returns 0, focus moves anyway.
  **F6 / Shift-F6** (and Ctrl-Tab, where the terminal sends it) always move focus, even out of a text area.
- `node->focused` and `node->hovered` are maintained for you; read them in draw callbacks.
- Global shortcuts: check a `VuiControls` table (`ctl_bind` / `ctl_dispatch`) before `vui_dispatch`, or with `vui_run` use its
  handler: it only sees events no widget used ("widgets get first refusal"). So Ctrl-C can be both: a text area copies when it has a
  selection (like Windows Terminal and most editors) and otherwise lets it through to your handler, which quits.
  To take a key *before* a widget, wrap its `on_event` (below) or check it in your own loop before `vui_dispatch`.

## Writing your own node

```c
typedef struct { VuiNode node; int clicks; } Counter;   // VuiNode must be first

static void draw(VuiNode *n, VuiScreen *scr, void *user) { /* draw inside n->rect, it is clipped */ }
static int on_event(VuiNode *n, VuiCtx *ui, const VuiEvent *e) { /* return 1 if handled */ }

Counter *c = (Counter *)node_alloc(sizeof(Counter));
c->node.draw = draw; c->node.on_event = on_event; c->node.focusable = 1;
```

`examples/focus-routing.c` does exactly this. Widgets in `src/widgets/` are built the same way.

## Customising built-in widgets

Everything a widget does is a public field on its struct, set after `*_create()`:

```c
VuiTextarea *ta = textarea_create(0);
ta->wrap = 0;                 // scroll sideways instead of wrapping
ta->tab_width = 2; ta->auto_indent = 1;
ta->node.wants_tab = 0;       // Tab moves focus instead of inserting
```

Styles are fields too (`label->style`, the theme for the rest). To change a behaviour that isn't a field, wrap the
widget's handler: `on_event` is a plain function pointer, so keep the old one and call it for what you don't handle:

```c
static int (*orig)(VuiNode *, VuiCtx *, const VuiEvent *);
static int mine(VuiNode *n, VuiCtx *ui, const VuiEvent *e)
{
    if (e->type == VUI_EVENT_KEY && e->key.key == VUI_KEY_ENTER) { /* your own Enter */ return 1; }
    return orig(n, ui, e);
}
orig = ta->node.on_event; ta->node.on_event = mine;
```

When nothing fits, build a node by hand (above): `draw`, `on_event`, `measure`, `destroy` and `userdata` are all it takes,
and it mixes freely with the built-ins in one tree. What isn't configurable yet: key bindings inside a widget (Ctrl-Z etc.
are fixed; wrap `on_event` to override one) and per-widget style on widgets that only read the theme.

## Testing

`make` builds all examples to `build/`; each one's header comment lists what to verify by hand, and
all of them must keep working. `make test` runs `tests/input.c` (decoder) and `tests/widgets.c`
(widget logic, layout, overlays, timers and fd watches against a stub backend, no terminal needed). `tests/screen.py` runs an example in a pty and prints the
rendered screen (`pip install pyte`), which is how most of this was checked without a human:

    python3 tests/screen.py build/text-input 12 50 -- 'hello' '\t' '\x1b[<0;5;3M'
