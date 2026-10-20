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
- Tab moves focus unless the focused node sets `wants_tab` and handles it (the text area does: use Shift-Tab, or Ctrl-Tab where the terminal sends it).
- `VuiInput` has a fixed capacity; both it and `VuiTextarea` store UTF-32. `VuiInput` has no undo or selection. The text area has no
  rectangular selection, search, line numbers or syntax anything; it finds lines by scanning the buffer (fine for
  thousands of lines, not huge files); undo history is dropped entirely past 64k characters / 4096 edits.
- Clipboard: copies go to an in-process clipboard and, via OSC 52, to the system one if the terminal
  allows it (some need it enabled, some cap the size). Reading the system clipboard is not possible
  from a terminal: use the terminal's paste (Ctrl-Shift-V), which arrives as a paste event.
- Overlays: at most 8, no tiling, no minimise/maximise, no keyboard move/resize, no menu bar with dropdown menus yet (dropdown, bar and window exist; menus would compose them). A new overlay's rect is fixed after it is added; content that grows later doesn't resize it. `list` measures its width from the first 256 rows.
- Event loop: 16 fd watches and 16 timers, level-triggered, millisecond resolution; no write-readiness watches, and a hung-up fd keeps firing until it is unwatched. A closed terminal (EOF on the tty) is not reported.
- Mouse drag capture: the node that handled a left press gets all mouse events until release.
- Focus/hover pointers are validated against the tree on each dispatch, but destroying a subtree
  while the app still holds raw pointers into it is on the app.

**Input**: see `input.md`: Ctrl+symbol / Ctrl+accented keys need a terminal with modifyOtherKeys or
the kitty protocol; no X10 mouse; no focus events; the Escape key has a 25 ms disambiguation delay.

**Platforms**
- Only the POSIX backend exists (Linux/macOS/BSD; NuttX untested). When stdin or stdout is a pipe the backend uses `/dev/tty`
  for keys and drawing (so `find | picker | xargs ...` works) and leaves stdout alone; it fails to init if there is no controlling terminal. Windows needs either a terminal
  that speaks VT sequences (Windows Terminal) with a Windows raw-mode backend, or a new backend: not started.
- vaporOS: the library only emits/reads standard VT sequences, so a terminal implementing the
  mouse, paste and key modes listed in `input.md` gets full functionality.

**Testing**
- `tests/screen.py` (pyte) checks text and colors, not that something *looks* right: highlight
  states were checked by attribute, not by eye. Re-check visually on real terminals when possible.
