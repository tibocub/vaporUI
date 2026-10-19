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
| `src/vaporui.c`, `src/ctx.h` | Context (size, screen, focus, hover), init/shutdown/poll |
| `src/platform/posix.c` | Backend: termios raw mode, SIGWINCH, poll/read, terminal mode switches. The only platform-specific file |
| `src/utf8.c`, `src/vt.c`, `src/input.c` | Byte -> UTF-8 -> escape sequences -> events (see `input.md`) |
| `src/screen.c` | Front/back cell buffers, diff flush, clip rect, SGR output |
| `src/node.c` | Tree, flexbox layout, clipped draw |
| `src/focus.c` | Focus, Tab traversal, hit testing, hover, event bubbling |
| `src/controls.c` | Flat key -> action table for app-wide shortcuts |
| `src/widgets/*.c` | One file per widget |

## Layout in one paragraph

Every node has `direction` (row/column), and along its parent's main axis `basis`, `grow`, `shrink`;
plus `gap` and uniform `padding` for its children. Cross axis always stretches. There is no
content-based sizing: you give widgets a `basis` or `grow`. `node->rect` is filled by `node_layout`
and is what draw callbacks and hit testing use.

## Events and focus

- `vui_dispatch` returns 1 if a node handled the event. Offered to `on_event` up the parent chain.
- Keys, text and paste go to the focused node. Mouse events go to the deepest node under the pointer
  (a left press also focuses the nearest focusable ancestor).
- Tab / Shift-Tab move focus through `focusable` nodes in tree order. A focused node with
  `wants_tab = 1` gets the Tab first (the text area does); if it returns 0, focus moves anyway. Ctrl-Tab always moves focus.
- `node->focused` and `node->hovered` are maintained for you; read them in draw callbacks.
- Global shortcuts: check a `VuiControls` table (`ctl_bind` / `ctl_dispatch`) before `vui_dispatch`.

## Writing your own node

```c
typedef struct { VuiNode node; int clicks; } Counter;   // VuiNode must be first

static void draw(VuiNode *n, VuiScreen *scr, void *user) { /* draw inside n->rect, it is clipped */ }
static int on_event(VuiNode *n, VuiCtx *ui, const VuiEvent *e) { /* return 1 if handled */ }

Counter *c = (Counter *)node_alloc(sizeof(Counter));
c->node.draw = draw; c->node.on_event = on_event; c->node.focusable = 1;
```

`examples/focus-routing.c` does exactly this. Widgets in `src/widgets/` are built the same way.

## Testing

`make` builds all examples to `build/`; each one's header comment lists what to verify by hand, and
all of them must keep working. `make test` runs `tests/input.c` (decoder) and `tests/widgets.c`
(widget logic, no terminal needed). `tests/screen.py` runs an example in a pty and prints the
rendered screen (`pip install pyte`), which is how most of this was checked without a human:

    python3 tests/screen.py build/text-input 12 50 -- 'hello' '\t' '\x1b[<0;5;3M'
