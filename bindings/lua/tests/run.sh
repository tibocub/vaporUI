#!/bin/sh
# usage: LUA=lua sh bindings/lua/tests/run.sh   (with the vaporui rock installed for that interpreter)
LUA=${LUA:-lua}; export LUA
cd "$(dirname "$0")/.." || exit 1
$LUA tests/snake_rules.lua || exit 1
python3 tests/pty_events.py || exit 1
python3 tests/pty_snake.py || exit 1
