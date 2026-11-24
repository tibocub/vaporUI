#!/bin/sh
# Real Windows console checks (docs/windows-test.md): runs vlr.exe inside a ConPTY (the console Windows Terminal uses) and feeds it
# keystrokes. Windows only; the plain suite is tests/run.sh. Usage: sh tests/win32/run.sh   (from Git Bash / MSYS2, after make)
cd "$(dirname "$0")/../.." || exit 1
VLR=${VLR:-./vlr.exe}
mkdir -p build
gcc -O2 -Wall -o build/conpty.exe tests/win32/conpty.c || exit 1
fail=0
T=${TEMP:-.}

# check NAME SCRIPT PATTERN... : steps on stdin; every PATTERN must appear in the ConPTY output (ESC and CR removed)
check() {
  name=$1 script=$2; shift 2
  out=$(build/conpty.exe 80 24 "$VLR" "$script" | tr -d '\033\r' )
  for p in "$@"; do
    case $out in *"$p"*) ;; *) echo "FAIL $name: missing '$p'"; printf '%s\n' "$out" | head -40; fail=1; return;; esac
  done
  echo "ok   $name"
}

# 1. keys, Ctrl+Right, lone ESC, resize, quit
check "events (keys, escape, resize)" tests/pty/events.lua "EV char,a" "EV key,up,0" "EV key,right,4" "EV key,escape,0" \
  "EV resize,100,30" "EV char,q" DONE <<'STEPS'
wait 500
send a
send \e[A
send \e[1;5C
send \e
wait 400
resize 100 30
wait 700
send q
wait 300
STEPS

# 1b. mouse: known gap, not hidden. A ConPTY does not hand SGR mouse bytes to an app that asked for mouse reporting with
#     vapor.input({mouse=1}) (they never arrive); a real Windows Terminal / conhost window must be checked by hand.
echo "SKIP mouse events through ConPTY (check by hand, docs/windows-test.md)"

# 2. console restored after vapor.input(false): cooked line input with echo
check "console restored (cooked read, echo)" tests/win32/restore.lua READY "hi" "GOT:hi" <<'STEPS'
wait 300
send q
wait 600
send hi\r
wait 600
STEPS

# 3. stdin from a pipe
printf 'print(io.read("l"))\n' > "$T/vlr-pipe.lua"
if [ "$(echo hi | "$VLR" "$T/vlr-pipe.lua" | tr -d '\r')" = hi ]; then echo "ok   stdin pipe"; else echo "FAIL stdin pipe"; fail=1; fi

# 4. non-ASCII: UTF-8 bytes on a console (ConPTY) and on a pipe, UTF-8 names through vapor.fs
check "utf8 (console)" tests/win32/utf8.lua "OUT é€" "NAMES x.txt,é€" "UTF8 OK" <<'STEPS'
wait 800
STEPS
if "$VLR" tests/win32/utf8.lua | grep -aq 'OUT é€'; then echo "ok   utf8 (pipe)"; else echo "FAIL utf8 (pipe)"; fail=1; fi

# 5. Ctrl+C: a key event inside vapor.input(true), ends the program outside it
check "ctrl+c inside vapor.input" tests/win32/ctrlc.lua "EV key,c,4" DONE <<'STEPS'
wait 500
send \x03
wait 400
STEPS
printf 'vapor.sleep(10)\n' > "$T/vlr-sleep.lua"
out=$(printf 'wait 500\nsend \x03\nwait 500\n' | build/conpty.exe 80 24 "$VLR" "$T/vlr-sleep.lua" | tr -d '\r' | tail -1)
if [ "$out" = "EXIT 3221225786" ]; then echo "ok   ctrl+c outside vapor.input ends the program"; else echo "FAIL ctrl+c outside input: $out"; fail=1; fi
exit $fail
