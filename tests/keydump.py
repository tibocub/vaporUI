#!/usr/bin/env python3
# Shows the raw bytes your terminal sends for each key press, so we know what it really says.
#   python3 tests/keydump.py            plain: what a legacy terminal sends
#   python3 tests/keydump.py kitty      also asks for the kitty keyboard protocol (flags 1+4)
#   python3 tests/keydump.py mok1|mok2  also asks for xterm modifyOtherKeys level 1 or 2
# Press keys, Ctrl+D (byte 04) quits. Send us the lines for the keys that misbehave (Alt+é, Ctrl+é, Alt+&, Ctrl+a...).
import os, sys, termios, tty

mode = sys.argv[1] if len(sys.argv) > 1 else "plain"
on = {"kitty": "\x1b[>5u", "mok1": "\x1b[>4;1m", "mok2": "\x1b[>4;2m"}.get(mode, "")
off = {"kitty": "\x1b[<u", "mok1": "\x1b[>4;0m", "mok2": "\x1b[>4;0m"}.get(mode, "")

fd = sys.stdin.fileno()
old = termios.tcgetattr(fd)
tty.setraw(fd)
try:
    os.write(1, (on).encode())
    os.write(1, f"mode: {mode} (Ctrl+D quits)\r\n".encode())
    while True:
        data = os.read(fd, 64)
        if data == b"\x04":
            break
        os.write(1, (data.hex(" ") + "   " + repr(data.decode("utf-8", "replace"))[1:-1] + "\r\n").encode())
finally:
    os.write(1, off.encode())
    termios.tcsetattr(fd, termios.TCSADRAIN, old)
