# Widgets

All created with `xxx_create(...)`, which returns a struct whose first member is the `VuiNode`
(`&w->node` to `node_add`). Fields marked public in `vaporui.h` can be set directly. Strings are
**not copied**: keep them alive as long as the widget. Widgets don't size from content: set
`node.basis` (size along the parent's main axis) or `node.grow`.

| Widget | Create | Behaviour |
|--------|--------|-----------|
| `VuiDiv` | `div_create(dir)` | Container that paints `style` as background |
| `VuiBorder` | `border_create(title)` | Frame (`border` = single/rounded/double, `style`, optional title); children go inside (padding 1) |
| `VuiButton` | `button_create(label, on_click)` | Focusable; Enter / Space / left click (on press) calls `on_click(&button->node)`. The whole rect is filled: dark normally, blue when hovered, reversed when focused |
| `VuiChecklist` | `checklist_create(items, count, radio)` | Up/Down/Home/End move, Space/Enter/click toggle; `radio` keeps exactly one item selected. Scrolls. Read `checked[i]`, get changes via `on_change(list, index)`. Hovered row is highlighted |
| `VuiInput` | `input_create(max_chars)` | Single-line text field, fixed capacity. See `vaporui.h` for the keys. `input_set_text`, `input_get_text`, `on_change`, `on_submit` |

Callbacks receive the widget's `VuiNode *`; use `node->userdata` for your own state.

## Conventions when adding a widget

- One file in `src/widgets/`, declaration in `vaporui.h` (struct with `VuiNode node;` first).
- State in the struct, `node_alloc(sizeof(...) [+ trailing array])`; no separate free needed.
- `draw` only touches `node->rect` (clipped anyway); look at `node->focused` / `node->hovered`.
- `on_event` returns 1 only for events it consumed, so the rest bubbles to parents and to your global shortcuts.
- Add or extend an example, and a case in `tests/widgets.c` for non-trivial logic.
