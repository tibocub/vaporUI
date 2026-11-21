# Widgets

All created with `xxx_create(...)`, which returns a struct whose first member is the `VuiNode`
(`&w->node` to `node_add`). Fields marked public in `vaporui.h` can be set directly. Strings are
**not copied**: keep them alive as long as the widget.

## Sizing

`node.basis` is the size along the parent's main axis; `node.grow` shares out what is left.
`VUI_AUTO` as a basis means "as big as my content": content widgets (label, button, checklist,
input) default to it and measure themselves (`node.measure`); a container with an `VUI_AUTO` basis
measures its children (sum along its direction + gaps, max across, + padding). Plain nodes default to
basis 0, so they only get space from `grow`. Cross-axis size stretches unless the parent sets `align`. Space nobody grows
into stays empty unless the parent sets `justify`. Custom nodes set `measure` to take part.

| Widget | Create | Behaviour |
|--------|--------|-----------|
| `VuiLabel` | `label_create(text)` | Static text, `\n` separates lines, `style` |
| `VuiDiv` | `div_create(dir)` | Container that paints `style` as background |
| `VuiBorder` | `border_create(title)` | Frame (`border` = single/rounded/double, `style`, optional title); children go inside (padding 1) |
| `VuiButton` | `button_create(label, on_click)` | Focusable; Enter / Space / left click (on press) calls `on_click(&button->node)`. The whole rect is filled in the theme's `text`, `hover` (pointer over it) or `focus` style |
| `VuiChecklist` | `checklist_create(items, count, radio)` | Up/Down/Home/End move, Space/Enter/click toggle; `radio` keeps exactly one item selected. Scrolls. Read `checked[i]`, get changes via `on_change(list, index)`. Hovered row is highlighted |
| `VuiInput` | `input_create(max_chars)` | Single-line text field, fixed capacity. See `vaporui.h` for the keys. `input_set_text`, `input_get_text`, `on_change`, `on_submit` |
| `VuiTextarea` | `textarea_create(max_chars)` | Multi-line editor on one growing buffer (0 = unlimited): selection (Shift+nav, mouse drag), clipboard, word wrap (`wrap`, default on), wide characters, undo/redo, optional `auto_indent`. Tab inserts spaces and the node sets `wants_tab`, so **Shift-Tab / Ctrl-Tab** leave it. `textarea_set_text/get_text/get_selection/select_all/undo/redo/cursor_pos`, `on_change`. Key list in `vaporui.h` |

| `VuiList` | `list_create(count, item)` | Virtual list: row text comes from `item(list, i)` when drawn, so any amount of data, nothing copied. Up/Down/PageUp/PageDown/Home/End, wheel, click. `on_select` (Enter / click), `on_cursor`. After the data changes call `list_set_count`; `list_set_cursor` moves the cursor |
| scroll container | `scroll_create()` (a plain `VuiNode`) | Children keep their natural size and scroll: wheel, arrows / PageUp / PageDown / Home / End when no child takes them, a clickable scrollbar; focusing a child scrolls it into view. Give children a `basis` (or `VUI_AUTO` content) |
| `VuiWindow` | `window_create(title)` | A DOS-style frame for an overlay: drag the title bar, resize from the bottom-right corner, `[x]` closes (`on_close`, else it removes itself). The top window has the `title` style, the others `title_off` |
| `VuiDropdown` | `dropdown_create(items, count, on_change)` | One-line selector; Enter / Space / Down / click opens a popup list (an overlay that dismisses on Escape or an outside click). `selected` holds the choice |
| `VuiMenubar` | `menubar_create(menus, count, on_pick)` | Menu titles in one row (put it in a `bar`); click a title or Left/Right + Enter/Down opens its items; Left/Right in the open menu switch menus. `on_pick(bar, menu, item)` |
| (context menu) | `menu_popup(x, y, items, count, on_pick, ud)` | Popup list at a point (e.g. on a right-click); kept on screen; Escape or an outside click closes it |
| bar | `bar_create()` (a `VuiDiv`) | One row in the theme's `bar` style, gap 1: put labels in it (give them `style = vui_theme()->bar`) for status or menu bars |

Callbacks receive the widget's `VuiNode *`; use `node->userdata` for your own state.

## Conventions when adding a widget

- One file in `src/widgets/`, declaration in `vaporui.h` (struct with `VuiNode node;` first).
- State in the struct, `node_alloc(sizeof(...) [+ trailing array])`. If the widget owns other memory, set `node.destroy` to free it (called by `node_destroy`).
- `draw` only touches `node->rect` (clipped anyway); look at `node->focused` / `node->hovered`.
- `on_event` returns 1 only for events it consumed, so the rest bubbles to parents and to your global shortcuts.
- Content widgets default to `basis = VUI_AUTO` and set `node.measure`.
- Add or extend an example, and a case in `tests/widgets.c` for non-trivial logic.

## Terminal (optional, POSIX only)
`terminal_create(argv)` runs a program (NULL = `$SHELL`) on a pty and draws its screen, using the vendored
libvterm (`third_party/libvterm`). It lives outside the default sources: `src/ext/terminal.c` plus the
libvterm objects (see the Makefile, which links them into the `windows` example and defines `VUI_TERMINAL`).
Every key goes to the program (Ctrl-C and Tab included); F6 / Shift-F6 move focus out. The pty follows the
node's size; `on_exit` fires when the program ends. Resizing re-wraps the shown rows (libvterm reflow) and keeps up to `VUI_TERMINAL_SCROLLBACK` (200) scrolled-off lines, which come back when the window grows (a too-wide line in pieces); lines that were split by a shrink are not re-joined when it grows again. Not done: combining marks (dropped when drawing).

Scrollback view: wheel (when the program didn't ask for the mouse), Shift-PageUp/PageDown, Shift-Home/End, or `terminal_scroll(t, rows)` / `terminal_scroll_reset(t)` / `terminal_scroll_offset(t)` (`t->scroll`). Typing to the program snaps back to live; new output keeps the view on the same text. Stored lines are shown at their own width (cut / padded, no reflow); the alternate screen has none. `VUI_TERMINAL_SCROLLBACK` is a compile-time knob (0 = no scrollback code). The ring costs O(1) per scrolled line.

Mouse: if the program enabled mouse reporting (and Shift isn't held), press / release / drag / wheel are forwarded. Otherwise left-drag selects (cell-wise, across scrollback; no word/line click yet), drawn inverted; `terminal_selection_text(t, out, size)` reads it (returns the full length, `out` may be NULL), `terminal_copy(t)` puts it on the clipboard (and OSC 52), `terminal_selection_clear(t)`.

Paste: a paste event (the host terminal's Ctrl-Shift-V) is forwarded, bracketed if the program enabled bracketed paste (libvterm does that), otherwise as plain text (newline as Enter). `terminal_paste(t)` pastes the in-process clipboard the same way (control characters stripped). There is no way to read the *system* clipboard from a terminal or the Linux VT / Windows console backends, so Ctrl-Shift-V inside vaporUI pastes only what was copied by this process; hosts that deliver a paste event give you the system clipboard for free.

Keys: `t->on_key(t, key)` runs before the widget's own handling (return 1 = consumed, not sent to the program); the defaults are Shift-PageUp/PageDown/Home/End (scroll) and Ctrl-Shift-C/V (copy/paste), so an app can rebind them by consuming its own keys, or by consuming these ones. Ctrl-Shift-C/V never reach the program, but a terminal without modifyOtherKeys / kitty sends Ctrl-Shift-C as Ctrl-C (0x03), which can't be told apart.
On NuttX (no MMU, so no `fork`) it opens `/dev/ptmx` and `posix_spawn`s the program: needs `CONFIG_PSEUDOTERM`,
enabled by vaporOS's `vaporui-terminal` feature (sim-nsh). libvterm keeps ~40 B per cell (twice with the alternate
screen), so it is not meant for the RP2040. Not for wasm / Windows.
