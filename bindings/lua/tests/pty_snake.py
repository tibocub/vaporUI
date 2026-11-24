# The snake example on a real pty: draws, moves, pauses, dies against a wall, restarts, quits, and restores the terminal (posix only).
import fcntl, os, pty, select, struct, sys, termios, time

lua = os.environ.get("LUA", "lua")
pid, fd = pty.fork()
if pid == 0:
    fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))
    os.execvp(lua, [lua, "examples/snake/main.lua"])
buf = b""

def pump(t):
    global buf
    end = time.time() + t
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if r:
            try: buf += os.read(fd, 65536)
            except OSError: return

def text(): return buf.decode("utf-8", "replace")

pump(0.6)
fail = []
if "\x1b[?1049h" not in text(): fail.append("alternate screen not entered")
if "Score 0" not in text(): fail.append("no score line")
if "██" not in text(): fail.append("no snake head")
buf = b""
os.write(fd, b"p"); pump(0.3)
if " paused " not in text(): fail.append("no pause banner")
os.write(fd, b"p"); buf = b""
pump(3.8)                         # runs right (20 cells at 0.16 s) until the wall: game over
if "game over" not in text(): fail.append("no game over after hitting the wall")
os.write(fd, b"r"); buf = b""; pump(0.4)
if "██" not in text(): fail.append("restart did not redraw the snake")
os.write(fd, b"\x1b[A"); pump(0.3)   # a turn must not crash
os.write(fd, b"q"); pump(0.5)
_, status = os.waitpid(pid, 0)
out = text()
if "\x1b[?1049l" not in out: fail.append("alternate screen not left")
if os.waitstatus_to_exitcode(status) != 0: fail.append("exit status %d" % os.waitstatus_to_exitcode(status))
if fail:
    print("FAIL pty snake:", fail); print(repr(out[-600:])); sys.exit(1)
print("ok   tests/pty snake (draw, pause, game over, restart, quit, terminal restored)")
