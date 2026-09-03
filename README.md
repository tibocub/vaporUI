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
*by rough order of priority -- higher = more priority*

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
