# Things to know

Current limits, in one place. Update this file when one is lifted.

**Rendering**
- 8 ANSI colors + bold / underline / reverse only. No 256/truecolor, no theming yet.
- Wide characters use a compact width table (CJK, fullwidth, common emoji = 2; combining marks = 0, and
  they are dropped, not overlaid). Terminals disagree on some emoji / ZWJ sequences: those can misalign.
- The terminal's cursor is hidden; input fields and the text area draw their own reversed cell.

**Layout**
- Content sizing (`VUI_AUTO`) is along the parent's main axis only; no height-for-width (a wrapping
  text area can't ask for "as many rows as its text needs").
- Cross axis is always stretch; justify is start-packed + gap; padding is uniform.

**Widgets / events**
- Buttons activate on mouse *press*, not release.
- Tab moves focus unless the focused node sets `wants_tab` and handles it (the text area does: use Shift-Tab, or Ctrl-Tab where the terminal sends it).
- `VuiInput` has a fixed capacity; both it and `VuiTextarea` store UTF-32. `VuiInput` has no undo or selection. The text area has no
  rectangular selection, search, line numbers or syntax anything; it finds lines by scanning the buffer (fine for
  thousands of lines, not huge files); undo history is dropped entirely past 64k characters / 4096 edits.
- Clipboard: copies go to an in-process clipboard and, via OSC 52, to the system one if the terminal
  allows it (some need it enabled, some cap the size). Reading the system clipboard is not possible
  from a terminal: use the terminal's paste (Ctrl-Shift-V), which arrives as a paste event.
- Mouse drag capture: the node that handled a left press gets all mouse events until release.
- Focus/hover pointers are validated against the tree on each dispatch, but destroying a subtree
  while the app still holds raw pointers into it is on the app.

**Input**: see `input.md`: Ctrl+symbol / Ctrl+accented keys need a terminal with modifyOtherKeys or
the kitty protocol; no X10 mouse; no focus events; the Escape key has a 25 ms disambiguation delay.

**Platforms**
- Only the POSIX backend exists (Linux/macOS/BSD; NuttX untested). Windows needs either a terminal
  that speaks VT sequences (Windows Terminal) with a Windows raw-mode backend, or a new backend: not started.
- vaporOS: the library only emits/reads standard VT sequences, so a terminal implementing the
  mouse, paste and key modes listed in `input.md` gets full functionality.

**Testing**
- `tests/screen.py` (pyte) checks text and colors, not that something *looks* right: highlight
  states were checked by attribute, not by eye. Re-check visually on real terminals when possible.
