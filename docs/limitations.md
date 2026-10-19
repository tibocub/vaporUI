# Things to know

Current limits, in one place. Update this file when one is lifted.

**Rendering**
- 8 ANSI colors + bold / underline / reverse only. No 256/truecolor, no theming yet.
- No wide (double-width) characters: every codepoint is one cell, so CJK/emoji misalign. Applies to
  `scr_puts`, the input field and button label centering (labels are assumed ASCII).
- No real terminal cursor: the input field draws its own reversed cell.

**Layout**
- No content-based sizing: give widgets `basis` / `grow`.
- Cross axis is always stretch; justify is start-packed + gap; padding is uniform.

**Widgets / events**
- Buttons activate on mouse *press*, not release.
- Tab always moves focus unless the focused node sets `wants_tab` and handles it (no shipped widget does yet).
- `VuiInput` stores fixed-capacity UTF-32; no undo, selection or multi-line (text area is not written).
- Focus/hover pointers are validated against the tree on each dispatch, but destroying a subtree
  while the app still holds raw pointers into it is on the app.

**Input**: see `input.md`: Ctrl+symbol / Ctrl+accented keys need a terminal with modifyOtherKeys or
the kitty protocol; no X10 mouse; no focus events; the Escape key has a 50 ms disambiguation delay.

**Platforms**
- Only the POSIX backend exists (Linux/macOS/BSD; NuttX untested). Windows needs either a terminal
  that speaks VT sequences (Windows Terminal) with a Windows raw-mode backend, or a new backend: not started.
- vaporOS: the library only emits/reads standard VT sequences, so a terminal implementing the
  mouse, paste and key modes listed in `input.md` gets full functionality.

**Testing**
- `tests/screen.py` (pyte) checks text and colors, not that something *looks* right: highlight
  states were checked by attribute, not by eye. Re-check visually on real terminals when possible.
