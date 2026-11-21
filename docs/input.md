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

## Key release and repeat

Terminals don't report a key going up, but two backends see it: the Linux console (raw keycodes) and the Windows
console. `vui_set_key_release(1)` opts in: you then get `VUI_EVENT_KEY` with `key.release == 1` for every key that
goes up, and `key == VUI_KEY_NONE` events (with the `scancode`, `VUI_SC_LALT` and so on) when a modifier goes down or
up, so "Alt still held" is `modifiers & VUI_MOD_ALT` on the latest event (modifiers are what is held *after* the event).
`key.repeat` is 1 for the auto-repeat of a held key (those two backends only, always on). `vui_dispatch()` ignores both
kinds of event, so widgets never see them: read them in your loop or the `vui_run` handler. Everywhere else (terminals
in a window, ssh) the option does nothing.

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
   `vui_set_layout("fr")` or `VUI_LAYOUT=fr` in the environment (`us`, the default, `fr`, `de`, `es`, `it`, `gb`; dead keys and AltGr characters have no entries; a table is a
   few lines in `src/input.c`, contributions welcome). A terminal that does send alternate keys never needs it.
3. **Windows:** exact, from the console itself. The backend reads `KEY_EVENT` records (virtual key, scan code, control
   state) and `MOUSE_EVENT` records natively, with no VT input mode and no escape sequences, so it does not depend on
   the terminal (Windows Terminal, conhost, ssh into it all deliver the same records). AltGr (Right Alt + Left Ctrl)
   that typed a character is text. Translation is in `src/winkeys.c` (pure, tested on every platform).

Every key event also carries `scancode`, the physical key (`VuiScancode`, USB HID numbering, so web, macOS and Linux
tables convert to it). It is exact where the platform reports it (Windows today) and derived from `base` elsewhere.

`Ctrl+é` and friends have no legacy encoding: they only arrive if the terminal speaks modifyOtherKeys or the kitty
protocol (tier 2-3 above). When they arrive, `base` works for them like for any key.


### Windows: what was checked on a real console, and known quirks

`tests/win_drive.c` types real keystrokes (`SendInput`, by scan code, so the layout does the translation) into a real console window and
reads back what `examples/input-handling` shows: `win_drive 0000040C build/input-handling.exe < chords.txt`, one chord per line
(`Ctrl+Alt+é`, `Alt+Shift+&`, `F5`...; keys are named by their AZERTY keycap; a leading `>` skips the Escape sentinel it types first, needed
to test dead keys). The first argument asks the window for a keyboard layout (`0000040C` French, `00000409` US). It stops rather than type
into another window when focus is lost. Checked: a conhost window (what cmd.exe and PowerShell both run in) on the French layout, 135 chords.

Works: Ctrl+letter and Ctrl+Shift+letter (`Ctrl-a [base q]`), Ctrl/Alt/Ctrl+Alt/Ctrl+Shift on the whole number row (`&é"'(-è_çà`), Alt+Shift+&
(`Alt-Shift-& [base 1]`), `é è à ç ù € @` as text, dead keys (`^`+`e` = `ê`, `¨`+`e` = `ë`, `~`+`n` = `ñ`), arrows / Home / End / Delete /
Backspace / Tab / Enter / F1-F12 with every modifier mix, scancodes. With the US layout loaded the same keys give the US characters.

Known quirks, not fixed:
- **AltGr + a dead key** (AltGr+é = `~` on AZERTY): an extra `Ctrl-Alt-<key>` event arrives, and its `character` is `é` in one window and the base
  digit (`2`) in another. Suspected cause: the console has already consumed the dead key when `unshifted_char` (`win32.c`) calls `ToUnicodeEx`, so
  it returns nothing and the base fallback is used. A dead key itself (`^`, `¨`) correctly produces nothing until its partner.
- **Ctrl+^ / Alt+^** report `Ctrl-[` / `Alt-[`: the key is dead and has no character, so the US base (`[`) is used, which also means Escape.
- **Ctrl+Shift+à** (Ctrl+Shift+0) never arrives. Windows or the console takes it; not investigated further.
- `unshifted_char` asks the layout of the *foreground* window: it is wrong when the console is not the focused window (and for ssh sessions).
- **Terminals that talk VT to ConPTY** (simulated by feeding bytes through a ConPTY, not with a real Ghostty): text (`é`, `€`), Alt+é, `ESC 2`
  (= `Alt-Shift-é`) and modified arrows work. Ctrl+é / Ctrl+symbol sent as kitty `CSI u` or modifyOtherKeys arrive as nothing (ConPTY drops them),
  Ctrl+Space (byte 0) is read as `Ctrl-à [base 0]`, and the scancode of ConPTY-made records differs from native ones (`sc=92` vs `80` for Left).
  The win32-input-mode sequences Windows Terminal sends could not be fed through the test ConPTY, so that path is unverified.
- **Not tested:** Windows Terminal and Ghostty themselves. A terminal's own shortcuts are handled before the program sees the key; Windows
  Terminal binds Ctrl+Shift+letters, Alt+Shift+-/+/D, Ctrl+Alt+digit, Alt+arrows, Ctrl+Tab, Alt+Enter, F11 and Ctrl+=/- by default (from its
  documentation, not checked here): if a Ctrl or Alt combo is missing there, change the terminal's bindings.
- Alt+Enter, Alt+Space, Alt+Tab, Alt+F4 and Ctrl+Esc belong to Windows or the console and were not tried.

### The Linux console (TTY)

The console speaks neither kitty nor modifyOtherKeys, and its keymap loses information (on a French keymap Alt+é and
Alt+Shift+é are both `ESC 2`, Ctrl+& is a plain `&`, Ctrl+é is `0x1e`, the same as Ctrl+^). So on a real VT (stdin
answers `KDGKBTYPE`) vaporUI does what games and `showkey` do: it puts the keyboard in **K_MEDIUMRAW**, where the kernel
reports which physical key went down or up, and translates itself (`src/vtraw.c`, pure and tested; the keymap is read
with `KDGKBENT`, the accent table with `KDGKBDIACRUC`). The result is exact: every Ctrl/Alt/Shift combination on every
key, with `scancode` and `base` like on Windows, dead keys and AltGr as typed, NumLock/CapsLock tracked by the app.

Consequences and safety:
- The keyboard mode belongs to the console and outlives the process, so it is restored at exit and on fatal signals,
  and a tiny guard process restores it if the app is killed outright (SIGKILL, crash). If a keyboard is ever left
  dead anyway: Alt+SysRq+R, or log in over ssh and `kbd_mode -u`.
- The kernel's console switching is off in raw mode, so Ctrl+Alt+F1..F12 are handled by the backend
  (`VT_ACTIVATE`); Alt+Fn is left to the app. Ctrl+Alt+Del does not reboot while the app runs.
- CapsLock/NumLock LEDs follow the app's state (KDSETLED, handed back to the kernel at exit); the Compose key makes the next character a dead key, like the kernel's (sequences are the accent table's, not X's Compose files). Not done: KT_DEAD2 accents; Shift+NumLock-style keypad inversions.
- `VUI_RAW_KEYS=0` goes back to the old way (bytes through the kernel keymap plus a `TIOCL_GETSHIFTSTATE` probe that
  recovers Alt+é, Ctrl+& and Ctrl+Shift+letter, but not Ctrl+é). It is also used when raw mode cannot be set up (no
  permission, no keymap we understand). Over ssh or in a terminal emulator there is no VT: the terminal tiers apply.

#### Mouse on the console (gpm)

The console has no mouse protocol; the `gpm` daemon owns the mouse and draws its own pointer and text selection. While an
app has the mouse on (`vui_set_mouse`), the POSIX backend connects to `/dev/gpmctl` (`src/gpm.c`, pure structs; no
libgpm needed), takes the events; the daemon keeps drawing its pointer for plain moves (a client can only draw one as root), but not while a button is held. Closing the socket (mouse off,
exit) gives the mouse back to gpm, so plain text selection works again outside the app. Without the daemon running
(`systemctl start gpm`) there is no mouse on a console. Shortcuts: the wheel's sign convention was read from gpm's
headers, not tested on hardware.

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
