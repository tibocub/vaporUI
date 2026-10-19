# vaporUI
A simple ncurses alternative for vaporOS.

Aims to be easier than ncurses and expose a
C ABI to be used from other languages such
as Lua.

## Portability
This library is optimized for portability and
cross-platform and might also offer a graphical
banckend to compile GUI out of the same code of
a TUI program and vice-versa (once the TUI part
is stable).

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
- `buttons-and-lists`: button, checklist, radio list

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
- [ ] wide/double-width character support
- [ ] 256-color / truecolor

Layout:
- [x] element tree (node_create/add/remove/destroy)
- [x] flexbox subset: direction, basis/grow/shrink, gap, uniform padding
- [x] wired to the renderer (node_draw walks the tree, draw callbacks get their node's computed rect)
- [ ] cross-axis alignment (currently stretch-only, no start/center/end)
- [ ] justify-content variants (currently packed-from-start + gap only)
- [ ] content-based sizing (basis is always explicit -- no text measurement yet)
- [ ] per-side padding (currently uniform only)
- [x] draw callbacks are clipped to their node's rect

Controls:
- [x] flat key -> action binding table (ctl_create/ctl_bind/ctl_dispatch)
- [x] Ctrl+<letter> decodes generally now, not just Ctrl-C (VUI_KEY_CHAR + VuiKeyEvent.character)
- [x] focus (Tab/Shift-Tab/click) and per-node `on_event` with bubbling to parents (`vui_dispatch`)
- [x] F1-F12, Insert (CSI n~ and SS3)
- [x] Alt (ESC prefix), Shift+Tab, xterm modified arrows/nav (CSI 1;m X), SS3 arrows, kitty CSI-u (any key + any modifier; enabled at init)
- [x] mouse (xterm SGR 1006: press/release/drag/wheel + modifiers; opt-in via vui_set_mouse)
- [x] bracketed paste (PASTE_START, TEXT..., PASTE_END -- streamed, no buffer)
- [ ] Ctrl+symbol/digit and Shift on non-letters only work on kitty-protocol terminals (no xterm modifyOtherKeys, no mouse yet)

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
- [ ] text area
- [ ] form
- [ ] tree
