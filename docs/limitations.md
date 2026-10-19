# Things to know

Current limits, in one place. Update this file when one is lifted.

**Rendering**
- 8 ANSI colors + bold / underline / reverse only. No 256/truecolor, no theming yet.
- No wide (double-width) characters: every codepoint is one cell, so CJK/emoji misalign. Applies to
  `scr_puts`, the input field and button label centering (labels are assumed ASCII).
- The terminal's cursor is hidden; input fields and the text area draw their own reversed cell.

**Layout**
- No content-based sizing: give widgets `basis` / `grow`.
- Cross axis is always stretch; justify is start-packed + gap; padding is uniform.

**Widgets / events**
- Buttons activate on mouse *press*, not release.
- Tab moves focus unless the focused node sets `wants_tab` and handles it (the text area does: use Shift-Tab, or Ctrl-Tab where the terminal sends it).
- `VuiInput` has a fixed capacity; both it and `VuiTextarea` store UTF-32 and have no undo, selection, clipboard copy, auto-indent or line wrapping. The text area finds lines by scanning the buffer, fine for thousands of lines, not for huge files.
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
