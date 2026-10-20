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
- [small-targets.md](docs/small-targets.md): RAM / flash / speed on the RP2040 and how it stays small
- [limitations.md](docs/limitations.md): everything that doesn't work yet, in one place

## Building
- **Linux / macOS / BSD:** `make` (needs `gcc`, `make`).
- **Windows, native:** install [w64devkit](https://github.com/skeeto/w64devkit) (a zip: unpack, run `w64devkit.exe`) or MSYS2 (`pacman -S mingw-w64-ucrt-x86_64-gcc make`), then `make` in the repo. `make` detects Windows and builds `.exe` files in `build/`.
- **Windows from Linux:** `make TARGET=WIN32 CC=x86_64-w64-mingw32-gcc`.
- Nothing else is needed: no NuttX, no external libraries. The Windows `.exe` files need Windows 10 or newer (VT console).

## Examples and tests
`make` builds every `examples/*.c` into `build/` They are both the
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
- `windows`: floating windows, modal dialog, drop-down, bars, scrolling, timers, themes
- `colors`: 16/256/truecolor, justify and align
- `picker`: fzf-like fuzzy finder (virtual list, `/dev/tty` UI when piped)
- `event-loop`: timers and a watched child-process pipe next to the keyboard

`make test` runs the automated checks (`tests/`): the input decoder, widgets, layout, overlays, timers and fd watches.
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
- [x] styling (bold/underline/reverse, colors)
- [x] theming (`vui_theme()`)
- [x] event loop: fd watches, timers, `vui_run`

Rendering:
- [x] cell buffer (double-buffered, diffed against what's on screen)
- [x] basic styling (8 ANSI colors, bold/underline/reverse)
- [x] wide/double-width character support (CJK, emoji table)
- [x] 256-color / truecolor (approximated down on terminals with less)

Layout:
- [x] element tree (node_create/add/remove/destroy)
- [x] flexbox subset: direction, basis/grow/shrink, gap, uniform padding
- [x] wired to the renderer (node_draw walks the tree, draw callbacks get their node's computed rect)
- [x] cross-axis alignment (stretch / start / center / end)
- [x] justify (start / center / end / between)
- [x] scrolling containers
- [x] content-based sizing (`VUI_AUTO`, per-node `measure`; main axis only)
- [x] per-side padding (`node_pad`)
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

Windows backend (`src/platform/win32.c`):
- [WIP] VT console mode, raw input, resize events, size: compiles with mingw and starts under Wine; untested on a real Windows console
- [x] fd watches on pipes (polled; sockets not supported); checked under Wine with `make wintest`

POSIX backend (linux/macOS/BSD/NuttX):
- [x] terminal resolution
- [x] resize events (SIGWINCH)
- [x] termios raw input/output

VaporUI widgets:
- [x] floating window (overlays: z-order, modal, dismiss; movable, resizable `window`)
- [x] popup (overlay with `VUI_OVERLAY_DISMISS`)
- [x] drop-down
- [x] bar (status bar; menu bars: `menubar_create`)
- [x] scroll container, virtual list
- [x] menu bar / context menu (`menubar_create`, `menu_popup`)
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
