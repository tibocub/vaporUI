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
Alt+x is `KEY`/`CHAR`/`ALT`, never text. A letter key is always the lowercase `character` plus `VUI_MOD_SHIFT` when
Shift is held (Alt+Shift+q is `'q'`, ALT|SHIFT, however the terminal reports it: `ESC Q`, `CSI 113;4u`...), like dwm's
`XK_q` + `ShiftMask`. Symbols can't be normalised: a terminal sends the shifted character (`!`, or `1` on AZERTY),
so bind what the layout types.

## What a terminal can actually send

A terminal only reports what it encodes. Three tiers, all handled in `src/vt.c` / `src/input.c`:

1. **Legacy (every terminal):** letters + Ctrl (as C0 bytes), Alt as an `ESC` prefix, arrows/Home/End/
   PgUp/PgDn/Insert/Delete/F1-F12, Ctrl/Shift/Alt on those via `CSI 1;<mod> X`, Shift-Tab.
   *Not representable:* Ctrl+digit, Ctrl+most symbols, Ctrl+accented letters, Shift+Enter...
2. **xterm modifyOtherKeys** (`CSI 27;<mod>;<code> ~`), enabled at level 2 (every modified key) when the terminal
   doesn't speak kitty: Ctrl+symbol / Ctrl+accented keys, Alt on any key.
3. **kitty keyboard protocol** (`CSI <code>[:<shifted>[:<base>]];<mod> u`), enabled at the "disambiguate" + "alternate keys"
   levels on init: any key with any modifiers, and which physical key it was.

At init vaporUI asks the terminal `CSI ? u` + `CSI c` (like neovim): a kitty reply selects the kitty protocol,
any other answer selects modifyOtherKeys 2, never both; a silent terminal stays legacy (100 ms wait). The mode is
undone at shutdown. In a terminal that knows neither, those combos simply never arrive.
`python3 tests/keydump.py [kitty|mok2]` prints the raw bytes a terminal sends per key, to see what it really says. Test with `examples/input-handling`: if a
combo doesn't show there, your terminal isn't sending it, not vaporUI failing to decode it.

### Non-QWERTY layouts (AZERTY etc.)

Characters are decoded as Unicode, so typing é, à, ç, ù, €, 日 works everywhere as `TEXT`. A key combo is a
`KEY` + `VUI_KEY_CHAR` with two ways to name the key, and you choose per binding:

- `character`: what the key types on the user's layout *without Shift*, and Shift is the modifier flag, like dwm's
  `XK_eacute` + `ShiftMask` and the kitty protocol's key code. Alt+Shift+É is `'é'` + ALT|SHIFT; so is Alt+Shift+é
  on AZERTY, which types a `2` (the terminal may say `ESC 2`; vaporUI turns it back into `'é'` + SHIFT). On US,
  Ctrl+$ is `'4'` + CTRL|SHIFT. Use it for "the key labelled Q".
- `base`: where the key sits, as the character of the same position on a US keyboard. The key typing `&` on AZERTY
  has base `'1'`, `é` is `'2'`, and Alt+Shift+that key is base `'1'` with SHIFT whichever way the terminal said it
  (`ESC 1`, since AZERTY types a 1 with Shift, or `ESC &` + a Shift flag). Use it for "the number row", so tag
  shortcuts work on every layout.

Where `base` comes from:
1. **The terminal**, with the kitty keyboard protocol's alternate keys (vaporUI asks for them at init): kitty, foot,
   WezTerm, Ghostty, Alacritty. Exact, nothing to configure.
2. **A layout table**, for every other terminal (GNOME Terminal, xterm, the Linux console...), which cannot tell:
   `vui_set_layout("fr")` or `VUI_LAYOUT=fr` in the environment (`us`, the default, and `fr` for now; a table is a
   few lines in `src/input.c`, contributions welcome). A terminal that does send alternate keys never needs it.
3. **Windows:** exact, from the console itself. The backend reads `KEY_EVENT` records (virtual key, scan code, control
   state) and `MOUSE_EVENT` records natively, with no VT input mode and no escape sequences, so it does not depend on
   the terminal (Windows Terminal, conhost, ssh into it all deliver the same records). AltGr (Right Alt + Left Ctrl)
   that typed a character is text. Translation is in `src/winkeys.c` (pure, tested on every platform).

Every key event also carries `scancode`, the physical key (`VuiScancode`, USB HID numbering, so web, macOS and Linux
tables convert to it). It is exact where the platform reports it (Windows today) and derived from `base` elsewhere.

`Ctrl+é` and friends have no legacy encoding: they only arrive if the terminal speaks modifyOtherKeys or the kitty
protocol (tier 2-3 above). When they arrive, `base` works for them like for any key.

### The Linux console (TTY)

The console speaks neither kitty nor modifyOtherKeys, and its keymap loses information. On a French keymap Alt+é and
Alt+Shift+é are both `ESC 2`, Ctrl+& is a plain `&`, and Ctrl+é is `0x1e` (Ctrl+^), the same as a real Ctrl+^.
vaporUI recovers what it can: when stdin is a real VT, each ESC-prefixed character, Ctrl+letter byte and typed
character is followed by a `TIOCL_GETSHIFTSTATE` ioctl that says which of Shift/Ctrl/Alt are down right now
(`vui_input_set_shift_probe`, installed by the POSIX backend). That restores Alt+é (`'é'` + ALT, base `'2'`, as
opposed to Alt+Shift+é = `'2'` + ALT|SHIFT), Ctrl+& (`'&'` + CTRL, base `'1'`) and Ctrl+Shift+letter. Not recoverable:
Ctrl+é and similar, where the kernel sends another key's control byte. Over ssh or in a terminal emulator there is no
VT to ask. Alt+F1..F6 switch virtual terminals in the kernel and never reach the app.

### Terminal shortcuts

vaporUI runs on the alternate screen (like vim, less, htop), so terminals that bind Shift+PageUp/Down,
Shift+Home/End etc. to scrollback usually leave them to the app. A terminal's own *application*
shortcuts (copy/paste, new tab, font size, user-configured bindings) are handled before the program
ever sees the key, and no escape sequence can override that: change the terminal's config. A vaporOS
terminal we write ourselves can simply not bind them.

### Known quirks

- Raw mode clears `ISIG`, `IXON` and `IEXTEN`: Ctrl-C/Z/\\ , Ctrl-S/Q and Ctrl-V reach the app as ordinary Ctrl keys (nothing is swallowed by the tty). Terminals themselves still keep Ctrl-Shift-C/V for their own copy/paste.
- A lone `Escape` is recognised after a 25 ms timeout (`VUI_ESCAPE_TIMEOUT_MS` in `posix.c`).
  `ESC` then a key sent much later arrives as two events.
- Legacy `Ctrl-H` is byte 8, which is also what many terminals send for Ctrl+Backspace.
  `Ctrl-I` is Tab, `Ctrl-M` is Enter, `Ctrl-[` is Escape: those are indistinguishable in tier 1.
- Typed text is plain: Shift on a letter gives uppercase `TEXT`. In a *combo* (Ctrl/Alt) a letter is always lowercase
  + SHIFT, in every tier.
- Not decoded yet: mouse X10/urxvt encodings (only SGR), focus in/out events, F13+, keypad.

## Mouse

`vui_set_mouse(ui, level)`: 0 off, 1 clicks + drag + wheel, 2 also plain motion (needed for
`node->hovered`; chatty). While on, the terminal's own text selection is disabled (usually
Shift+drag still selects). Uses xterm SGR reporting (1000/1002/1003 + 1006), which every modern
terminal speaks. For vaporOS, a terminal or a GPM-like daemon that emits the same sequences works
with no library change.
