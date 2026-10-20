# Small targets (RP2040 and friends)

vaporUI is built to fit boards like the RP2040 (264 KB RAM, 2 MB flash). Measured on a Waveshare RP2040-Zero
running vaporOS (NuttX) over its USB serial console, 80x24, examples from `vaporOS/vaporui` (`vui_windows`, `vui_colors`, ...):

| | |
|---|---|
| Flash | the library + six examples ~48 KB (vaporOS image 608 KB of 2 MB) |
| Static RAM | ~0 (a theme, one pointer, the colour depth) |
| Heap, screen | 4 bytes per cell, double-buffered: 80x24 = 15 KB (it was 61 KB with 16-byte cells), 120x40 = 38 KB |
| Heap, nodes | 120 B per node, `VuiCtx` 888 B; a text area 184 B + 4 B per character |
| Heap, peak | `vui_colors` ~17 KB, `vui_windows` with 3 windows and a 2000-character paste ~46 KB |
| Output buffer | 512 B on NuttX (`VUI_OUT_BUFFER`; 4 KB elsewhere, 16 KB on Windows) |
| Stack | 4 KB is enough for every example (deepest chain: draw recursion + `snprintf`) |
| Text area, full | 8192 characters (the NuttX default cap): heap peak ~67 KB over idle with undo history (32 KB text + 8 KB history + screen) |
| Pasting | 8086 characters, sent at 1.6 KB/s, keep up with the sender (5.4 s); frames are limited to one per 40 ms while input flows |
| Speed | full 80x24 frame 64 ms (6 KB over USB), a window drag step 17 ms, a keystroke 12 ms |

How it stays small:
- A cell is a codepoint plus an 8-bit id into a per-screen style table (255 styles on screen at once; extras draw with
  the default style, unused ones are reclaimed at the start of a frame).
- Only the diff against what the terminal shows is sent. Output goes through a small stdio buffer.
- Widgets allocate when created, nothing is reserved up front. Lists are virtual, so 200k rows cost nothing.

NuttX specifics, handled in `src/platform/posix.c`:
- A serial console has no window size: the size is asked from the terminal once at start (cursor to the far corner +
  `ESC[6n`), so the terminal must answer; there is no resize event.
- `stdout` can't be reassigned, so the "UI on /dev/tty when piped" trick is off: the UI uses stdin/stdout.
- Statics survive between runs of a builtin app: `vui_shutdown` puts the theme back and frees the clipboard, and examples
  must not rely on zero-initialised statics at the start of `main`.

Building on vaporOS: `vaporOS/vaporui` is a NuttX app that compiles `../vaporUI` and registers the examples as commands
(`make -f dev.mk build BOARD=rp2040`, enable with `CONFIG_VAPOROS_VAPORUI=y`).

Things that bit on the board (keep them in mind for new widgets):
- Anything that scans a whole line/buffer per keystroke is quadratic when pasting; keep per-key work bounded by what is on screen.
- Draw at most every `VUI_MIN_FRAME_MS` while input flows (`vui_run` / `vui_next_event` do it); a full frame is ~60 ms here.
- The USB console has a 513-byte input buffer and no flow control: a burst faster than the app drains is dropped, and very large
  bursts have wedged the console. Feed it in small chunks when testing.
- Ctrl-Q / Ctrl-S may be eaten as flow control by the serial driver on NuttX: bind quit to Ctrl-C (after widgets) instead.
