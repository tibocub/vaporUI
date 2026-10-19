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
- [ ] draw callbacks aren't clipped to their node's rect yet

Controls:
- [x] flat key -> action binding table (ctl_create/ctl_bind/ctl_dispatch)
- [x] Ctrl+<letter> decodes generally now, not just Ctrl-C (VUI_KEY_CHAR + VuiKeyEvent.character)
- [ ] per-node/focus-scoped controls (needs a real focusable widget first -- see widgets below)
- [x] Alt (ESC prefix), Shift+Tab, xterm modified arrows/nav (CSI 1;m X), SS3 arrows, kitty CSI-u (any key + any modifier; enabled at init)
- [ ] Ctrl+symbol/digit and Shift on non-letters only work on kitty-protocol terminals (no xterm modifyOtherKeys, no F-keys, no mouse yet)

POSIX backend (linux/macOS/BSD/NuttX):
- [x] terminal resolution
- [x] resize events (SIGWINCH)
- [x] termios raw input/output

VaporUI widgets:
- [ ] floating window
- [ ] popup
- [ ] drop-down
- [ ] bar (maybe distinct menubar/status bar widgets)
- [ ] border
- [ ] div
- [ ] push button
- [ ] radio button
- [ ] check-list
- [ ] text area
- [ ] form
- [ ] tree
