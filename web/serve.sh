#!/bin/sh
# Serve the browser build: web/ plus build/wasm/ as /wasm/. Open http://localhost:8080/?app=windows
cd "$(dirname "$0")" && rm -rf site && mkdir site && cp -r index.html dist site/ && cp -r ../build/wasm site/wasm
cd site && exec python3 -m http.server 8080
