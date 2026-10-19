# Input

## Events

`vui_poll_event` returns one `VuiEvent`:

| Type | Payload | Notes |
|------|---------|-------|
| `VUI_EVENT_TEXT` | `text.codepoint` | A typed character, already UTF-8 decoded. Plain letters, accents, symbols |
| `VUI_EVENT_KEY` | `key.key`, `key.modifiers`, `key.character` | Named keys (Enter, arrows, F1-F12...), and any character pressed *with* Ctrl or Alt as `VUI_KEY_CHAR` |
| `VUI_EVENT_MOUSE` | `mouse.button/action/modifiers/x/y` | Cell coordinates, 0-based. Needs `vui_set_mouse` |
| `VUI_EVENT_PASTE_START` / `_END` | none | Pasted text arrives in between as `TEXT` events (newline as `'\n'`, nothing triggers shortcuts) |
| `VUI_EVENT_RESIZE` | `size` | The screen buffer is already resized when you get it |
| `VUI_EVENT_NONE` | | Nothing complete yet; poll again |

Rule of thumb: a plain `q` is `TEXT`, `Ctrl-q` is `KEY` + `VUI_KEY_CHAR` + `VUI_MOD_CTRL` + `'q'`.
Alt+x is `KEY`/`CHAR`/`ALT`, never text. `character` is lowercase for Ctrl combos.

## What a terminal can actually send

A terminal only reports what it encodes. Three tiers, all handled in `src/vt.c` / `src/input.c`:

1. **Legacy (every terminal):** letters + Ctrl (as C0 bytes), Alt as an `ESC` prefix, arrows/Home/End/
   PgUp/PgDn/Insert/Delete/F1-F12, Ctrl/Shift/Alt on those via `CSI 1;<mod> X`, Shift-Tab.
   *Not representable:* Ctrl+digit, Ctrl+most symbols, Ctrl+accented letters, Shift+Enter...
2. **xterm modifyOtherKeys** (`CSI 27;<mod>;<code> ~`), enabled at level 1 on init: terminals that
   support it report Ctrl+symbol / Ctrl+accented keys.
3. **kitty keyboard protocol** (`CSI <code>;<mod> u`), enabled at the "disambiguate" level on init:
   any key with any modifiers.

Both modern modes are requested at init and undone at shutdown; terminals that don't know them ignore
the request, and then those combos simply never arrive. Test with `examples/input-handling`: if a
combo doesn't show there, your terminal isn't sending it, not vaporUI failing to decode it.

### Non-QWERTY layouts (AZERTY etc.)

Characters are decoded as Unicode, so typing é, à, ç, ù, €, 日 works everywhere as `TEXT`. The
shortcut that is the "Ctrl-2" of a QWERTY board is `Ctrl-é` on AZERTY, and it is reported as
`CHAR` + `CTRL` + `0xE9` (`'é'`), i.e. the character the key produces on your layout, *if the terminal
supports tier 2 or 3*. Bind it with `ctl_bind(c, VUI_KEY_CHAR, VUI_MOD_CTRL, 0xE9, ...)`.
On Windows, AltGr is reported by Windows as Ctrl+Alt; not tested yet.

### Known quirks

- A lone `Escape` is recognised after a 50 ms timeout (`VUI_ESCAPE_TIMEOUT_MS` in `posix.c`).
  `ESC` then a key sent much later arrives as two events.
- Legacy `Ctrl-H` is byte 8, which is also what many terminals send for Ctrl+Backspace.
  `Ctrl-I` is Tab, `Ctrl-M` is Enter, `Ctrl-[` is Escape: those are indistinguishable in tier 1.
- Shift on letters is not reported as a modifier (you get the uppercase `TEXT`); Shift on named keys
  and, in tiers 2-3, on non-text keys is.
- Not decoded yet: mouse X10/urxvt encodings (only SGR), focus in/out events, F13+, keypad.

## Mouse

`vui_set_mouse(ui, level)`: 0 off, 1 clicks + drag + wheel, 2 also plain motion (needed for
`node->hovered`; chatty). While on, the terminal's own text selection is disabled (usually
Shift+drag still selects). Uses xterm SGR reporting (1000/1002/1003 + 1006), which every modern
terminal speaks. For vaporOS, a terminal or a GPM-like daemon that emits the same sequences works
with no library change.
