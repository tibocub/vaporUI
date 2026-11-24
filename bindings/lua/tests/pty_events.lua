-- run by pty_events.py inside a pseudo terminal: prints every event as one line until "q" is typed
local ui = require("vaporui")
local cols, rows = assert(ui.open({mouse = tonumber(arg and arg[1]) or 2}))
io.stderr:write("OPEN ", cols, "x", rows, "\n")
while true do
  local ev = {ui.poll(5)}
  if ev[1] == nil then io.stderr:write("timeout\n"); break end
  io.stderr:write("EV ", table.concat(ev, ","), "\n")
  if ev[1] == "char" and ev[2] == "q" then break end
end
ui.close()
io.stderr:write("DONE\n")
