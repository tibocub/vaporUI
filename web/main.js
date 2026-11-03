// Page glue: a wterm terminal on one side, a vaporUI example compiled to wasm on the other.
// ?app=windows picks the example (build/wasm/<app>.js).
import { WTerm } from "@wterm/dom";
import "@wterm/dom/css";

const app = new URLSearchParams(location.search).get("app") || "windows";
const term = new WTerm(document.getElementById("terminal"), { cursorBlink: true });
await term.init();

// What wasm.c reads through Module.vui: a byte queue filled by the terminal, plus a promise that
// parks the wasm stack (Asyncify) until there is something to do.
const queue = [];
let wake = null, resized = false, closed = false;
const poke = (v) => { if (wake) { const w = wake; wake = null; w(v); } };

term.onData = (s) => { queue.push(...new TextEncoder().encode(s)); poke(1); };
term.onResize = () => { resized = true; poke(1); };

const vui = {
  wait: (ms) => closed ? -1 : queue.length || resized ? 1 : new Promise((res) => {
    wake = res;
    if (ms >= 0) setTimeout(() => { if (wake === res) { wake = null; res(0); } }, ms);
  }),
  pending: () => queue.length,
  take: (dst) => { const n = Math.min(dst.length, queue.length); for (let i = 0; i < n; i++) dst[i] = queue.shift(); return n; },
  resized: () => { const r = resized; resized = false; return r ? 1 : 0; },
  cols: () => term.cols,
  rows: () => term.rows,
};

// stdout arrives one byte at a time (signed); batch them into one write per tick.
let out = [];
const flush = () => { if (out.length) { term.write(new Uint8Array(out)); out = []; } };
const stdout = (b) => { if (b === null) return flush(); if (!out.length) queueMicrotask(flush); out.push(b & 0xff); };

term.focus();
const script = document.createElement("script");
script.src = `wasm/${app}.js`;
script.onload = async () => {
  await createVui({
    vui,
    preRun: [(m) => m.FS.init(() => null, stdout, (b) => b && console.error(String.fromCharCode(b & 0xff)))],
    onExit: () => { closed = true; flush(); term.write("\r\n[program exited]\r\n"); },
  });
};
document.body.append(script);
