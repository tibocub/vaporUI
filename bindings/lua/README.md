# vaporui (Lua binding)

Terminal input events and a double-buffered cell screen for Lua 5.1 to 5.4 and LuaJIT, on vaporUI's own platform layer
(Linux console and terminals, macOS and Windows consoles).

```lua
local ui = require("vaporui")
local cols, rows = ui.open{mouse = 1}          -- raw terminal, alternate screen
local scr = ui.screen(cols, rows)
scr:puts(1, 1, "hello, q quits", {fg = ui.color.green, bold = true}); scr:flush()
while true do
  local ev, a, b = ui.poll(1)                  -- "char", text | "key", name, mods, base, scancode | "mouse", action, button, x, y, mods
  if ev == "char" and a == "q" then break end  -- "resize", cols, rows | "paste", "start"/"end" | "quit" | nil, "timeout"
end
ui.close()
```

`poll` blocks the whole process while it waits, like any call in a single-threaded Lua program. Coordinates are 1-based.
The module header in `src/lvaporui.c` is the reference.

Install from the repository root: `luarocks make vaporui-scm-1.rockspec`.
Tests (need a Unix pty and the rock installed): `LUA=lua sh bindings/lua/tests/run.sh`; `examples/snake` is a small game.
