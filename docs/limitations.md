# Things to know

Current limits, in one place. Update this file when one is lifted.

**Rendering**
- Colors beyond the terminal's depth are approximated (truecolor -> 256 -> 16 by nearest cube / bit thresholds, not perceptual). No italic, strikethrough, dim, blink or underline colors.
- The theme is one global; widgets read it when they draw (zero `VuiStyle` fields in widget structs mean "terminal default", not "theme": copy from `vui_theme()` yourself).
- Wide characters use a compact width table (CJK, fullwidth, common emoji = 2; combining marks = 0, and
  they are dropped, not overlaid). Terminals disagree on some emoji / ZWJ sequences: those can misalign.
- The terminal's cursor is hidden; input fields and the text area draw their own reversed cell.

**Layout**
- Content sizing (`VUI_AUTO`) is along the parent's main axis only; no height-for-width (a wrapping
  text area can't ask for "as many rows as its text needs").
- No `align-self`, wrapping (`flex-wrap`), cross-axis basis, min/max sizes, or space-around/evenly. Cross-axis `align` other than stretch sizes children from their content, which is 0 for nodes without `measure`.
- Scrolling containers scroll along their main axis only.

**Widgets / events**
- Buttons activate on mouse *press*, not release.
- F6 / Shift-F6 always move focus. Tab moves focus unless the focused node sets `wants_tab` and handles it (the text area does: use Shift-Tab, or Ctrl-Tab where the terminal sends it).
- `VuiInput` has a fixed capacity; both it and `VuiTextarea` store UTF-32. `VuiInput` has no undo or selection. The text area has no
  rectangular selection, search, line numbers or syntax anything; it finds lines by scanning the buffer (fine for
  thousands of lines, not huge files); undo history is dropped entirely past 64k characters / 4096 edits.
- Clipboard: copies go to an in-process clipboard and, via OSC 52, to the system one if the terminal
  allows it (some need it enabled, some cap the size). Reading the system clipboard is not possible
  from a terminal: use the terminal's paste (Ctrl-Shift-V), which arrives as a paste event.
- Overlays: at most 8, no tiling, no minimise/maximise, no keyboard move/resize, menus (`menubar`, `menu_popup`) have no separators, shortcuts, disabled items, checkmarks or submenus, and clicking another title while one is open just closes it. A new overlay's rect is fixed after it is added; content that grows later doesn't resize it. `list` measures its width from the first 256 rows.
- Event loop: 16 fd watches and 16 timers, level-triggered, millisecond resolution; no write-readiness watches, and a hung-up fd keeps firing until it is unwatched. A closed terminal (EOF / hangup on the tty) is reported as `VUI_EVENT_CLOSE` on POSIX (`vui_run` quits on it; own loops should break); not on Windows.
- Mouse drag capture: the node that handled a left press gets all mouse events until release.
- Focus/hover pointers are validated against the tree on each dispatch, but destroying a subtree
  while the app still holds raw pointers into it is on the app.

**Terminal-owned keys**: a terminal's own key bindings (Windows Terminal: Ctrl-Shift-C/V/F/P/T, Shift-PgUp/PgDn, Alt-Enter, F11;
GNOME Terminal / Konsole: Ctrl-Shift-letters, Shift-PgUp/PgDn on the primary screen) are handled before the app ever sees the key.
The alternate screen only frees the scroll keys on terminals that tie them to the primary screen; the rest has to be unbound in the
terminal's settings. Keep essential app keys off those combinations.

**Input**: see `input.md`: Ctrl+symbol / Ctrl+accented keys need a terminal with modifyOtherKeys or
the kitty protocol; no X10 mouse; no focus events; the Escape key has a 25 ms disambiguation delay.

**Platforms**
- Backends: POSIX (Linux/macOS/BSD; NuttX untested) and Windows (`src/platform/win32.c`, VT console mode; cross-built with
  `make TARGET=WIN32 CC=x86_64-w64-mingw32-gcc`). The Windows one has only been compiled and run under Wine (input loop, Ctrl-C and the pipe watch
  verified; Wine's console doesn't render VT faithfully, so the drawing is unverified), not run on a real Windows console.
  Windows differences: fd watches work for CRT fds that are pipes (polled every 10 ms while any is watched; sockets can't be watched; other kinds always report ready), `event-loop` isn't built (fork / pipe / SIGCHLD). `make wintest` + wine checks the pipe watch.
  Consoles without VT support (pre-Windows 10) are not supported. The POSIX one is the reference.
- When stdin or stdout is a pipe, both backends draw on and read from the terminal (`/dev/tty`, `CONIN$`/`CONOUT$`) and leave
  stdout alone (so `find | picker | xargs ...` works); init fails if there is no terminal at all.
- vaporOS: the library only emits/reads standard VT sequences, so a terminal implementing the
  mouse, paste and key modes listed in `input.md` gets full functionality.

**Testing**
- `tests/screen.py` (pyte) checks text and colors, not that something *looks* right: highlight
  states were checked by attribute, not by eye. Re-check visually on real terminals when possible.
