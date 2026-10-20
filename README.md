# vaporUI
A simple and cross-platform ncurses alternative made for [vaporOS](https://github.com/tibocub/vaporOS).

Aims to be easier than ncurses and expose a
C ABI to be used from other languages such
as Lua.

## Portability
This library is optimized for portability and
cross-platform and might also offer a graphical
banckend to compile GUI out of the same code of
a TUI program and vice-versa (once the TUI part
is stable).

## Docs
- [overview.md](docs/overview.md): how an app is built, the pipeline, file map, custom nodes, testing
- [input.md](docs/input.md): events, what terminals can send, AZERTY/non-QWERTY notes, mouse
- [widgets.md](docs/widgets.md): widget reference and conventions for adding one
- [limitations.md](docs/limitations.md): everything that doesn't work yet, in one place

## Examples and tests
`make` builds every `examples/*.c` into `build/`. They are both the
how-to for building UIs with vaporUI and the manual regression suite:
each file's header comment lists what to verify, and every example
must keep working as features land. New feature or widget -> new
example (or extend one).

- `input-handling`: keys, modifiers, mouse, paste, resize, global controls
- `layout-resize`: nested flexbox panes, live resize
- `focus-routing`: custom nodes, focus, event bubbling
- `borders`: border and div containers
- `buttons-and-lists`: button, checklist, radio list, hover
- `text-input`: single-line input fields
- `text-area`: multi-line editor (selection, clipboard, wrap, undo)
- `auto-sizing`: content-based sizing, labels

`make test` runs the automated checks (`tests/`): the input decoder.
`tests/screen.py build/<example> ROWS COLS -- '\t' 'text' ...` runs an example in a pty, sends keys/mouse
sequences, and prints the rendered screen (needs `pip install pyte`).

## TODO
VaporUI API:
- [WIP] init
- [WIP] clear
- [WIP] get_size
- [WIP] shutdown
- [WIP] keyboard events
- [x] resize events
- [x] UTF-8 parser
- [x] VT parser
- [WIP] styling (bold/underline/colors/background etc.)
- [ ] theming

Rendering:
- [x] cell buffer (double-buffered, diffed against what's on screen)
- [x] basic styling (8 ANSI colors, bold/underline/reverse)
- [x] wide/double-width character support (CJK, emoji table)
- [ ] 256-color / truecolor

Layout:
- [x] element tree (node_create/add/remove/destroy)
- [x] flexbox subset: direction, basis/grow/shrink, gap, uniform padding
- [x] wired to the renderer (node_draw walks the tree, draw callbacks get their node's computed rect)
- [ ] cross-axis alignment (currently stretch-only, no start/center/end)
- [ ] justify-content variants (currently packed-from-start + gap only)
- [x] content-based sizing (`VUI_AUTO`, per-node `measure`; main axis only)
- [ ] per-side padding (currently uniform only)
- [x] draw callbacks are clipped to their node's rect

Controls:
- [x] flat key -> action binding table (ctl_create/ctl_bind/ctl_dispatch)
- [x] Ctrl+<letter> decodes generally now, not just Ctrl-C (VUI_KEY_CHAR + VuiKeyEvent.character)
- [x] focus (Tab/Shift-Tab/click) and per-node `on_event` with bubbling to parents (`vui_dispatch`)
- [x] F1-F12, Insert (CSI n~ and SS3)
- [x] Alt (ESC prefix), Shift+Tab, xterm modified arrows/nav (CSI 1;m X), SS3 arrows, kitty CSI-u (any key + any modifier; enabled at init)
- [x] mouse (xterm SGR 1006: press/release/drag/wheel/hover + modifiers; opt-in via vui_set_mouse)
- [x] bracketed paste (PASTE_START, TEXT..., PASTE_END -- streamed, no buffer)
- [x] xterm modifyOtherKeys + kitty protocol (Ctrl+accent/symbol on AZERTY etc.); legacy terminals can't send these, see docs/input.md

POSIX backend (linux/macOS/BSD/NuttX):
- [x] terminal resolution
- [x] resize events (SIGWINCH)
- [x] termios raw input/output

VaporUI widgets:
- [ ] floating window
- [ ] popup
- [ ] drop-down
- [ ] bar (maybe distinct menubar/status bar widgets)
- [x] border
- [x] div
- [x] push button
- [x] radio button
- [x] check-list
- [x] text input (single line)
- [x] text area (multi-line: selection, clipboard, wrap, undo/redo, auto-indent, wide chars)
- [x] label
- [ ] form
- [ ] tree
