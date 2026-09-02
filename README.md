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
- [ ] styling (bold/underline/colors/background etc.)
- [ ] theming

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
