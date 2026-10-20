# Drives an example in a pty and prints the rendered screen (needs `pip install pyte`).
# usage: pty.py build/example [rows cols] -- key1 key2 ...   (keys use python escapes, e.g. '\x1b[<0;5;5M')
import os, pty, select, struct, fcntl, termios, sys, pyte, codecs, time

def run(exe, keys, rows=24, cols=80, wait=0.3):
    pid, fd = pty.fork()
    if pid == 0:
        os.execv(exe, [exe])
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
    scr = pyte.Screen(cols, rows)
    st = pyte.ByteStream(scr)
    def rd(t):
        end = time.time() + t * 3 # animated UIs never go quiet: stop after a bounded time
        while time.time() < end and select.select([fd], [], [], t)[0]:
            try: d = os.read(fd, 65536)
            except OSError: return
            if not d: return
            st.feed(d)
    rd(0.5)
    for k in keys:
        os.write(fd, k); rd(wait)
    snap = [l.rstrip() for l in scr.display]
    try: os.kill(pid, 9)
    except OSError: pass
    os.waitpid(pid, 0)
    return snap

if __name__ == "__main__":
    a = sys.argv[1:]
    exe = a[0]; i = a.index("--") if "--" in a else len(a)
    rows, cols = (int(a[1]), int(a[2])) if i == 3 else (24, 80)
    keys = [codecs.escape_decode(k.encode())[0] for k in a[i+1:]]
    print("\n".join(run(exe, keys, rows, cols)))
