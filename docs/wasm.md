# Running in the browser (WebAssembly)

The examples compile to wasm with [emscripten](https://emscripten.org) and run in a web terminal ([wterm](https://github.com/vercel-labs/wterm)).

    source ~/Code/emsdk/emsdk_env.sh
    make TARGET=WASM              # build/wasm/<example>.js + .wasm
    cd web && npm i && bun build main.js --outdir dist --target browser
    ./serve.sh                    # http://localhost:8080/?app=windows

## How it works
- `src/platform/wasm.c` is the backend (same interface as posix.c / win32.c). Everything above it is unchanged.
- Output: the usual escape sequences go to `stdout`; `web/main.js` installs a stdout handler that feeds them to the terminal.
- Input: the terminal's keystrokes (`onData`) are queued in JS; the backend reads them through `Module.vui` and the normal input decoder parses them.
- Blocking: `-sASYNCIFY` lets `poll_event` wait on a JS promise without blocking the page.

## Limits
- No fd watches (`vui_watch_fd` never fires): there are no pipes or child processes. Timers work. `event-loop` is skipped.
- `picker` works as a normal full-screen app; there is no stdin pipe to filter.
- Asyncify makes the wasm bigger and slower (every function that can be on the stack while waiting is instrumented).
