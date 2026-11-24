-- Snake in the terminal on vaporui (the cell screen and input events). Run: lua main.lua   (needs the vaporui rock)
-- Arrows or WASD move, p pauses, r restarts, q or Esc quits. Runs unchanged on Linux, macOS and Windows consoles and in the browser terminal.
local here = (arg and arg[0] or ""):match("^(.*)[/\\]") or "."
package.path = here .. "/?.lua;" .. package.path
local ui = require("vaporui")
local Game = require("game")

local now = ui.now -- seconds
local C = ui.color
local st = {
  border = {fg = C.gray},
  body = {fg = C.green},
  head = {fg = C.bright_green, bold = true},
  food = {fg = C.red, bold = true},
  text = {fg = C.white},
  dim = {fg = C.gray},
  over = {fg = C.bright_red, bold = true},
}

local KEYS = {up = "up", down = "down", left = "left", right = "right", w = "up", s = "down", a = "left", d = "right", k = "up", j = "down", h = "left", l = "right"}

local cols, rows = ui.open({mouse = 0})
local scr = ui.screen(cols, rows)
local game, paused, best = nil, false, 0

-- every game cell is two columns wide, so the board looks square
local function new_game()
  local w = math.max(8, math.min(40, math.floor((cols - 2) / 2)))
  local h = math.max(6, math.min(24, rows - 4))
  game, paused = Game.new(w, h), false
end

local function draw()
  scr:clear()
  if cols < 20 or rows < 10 then scr:puts(1, 1, "window too small", st.over); scr:flush(); return end
  local bw, bh = game.w * 2 + 2, game.h + 2
  local ox, oy = math.floor((cols - bw) / 2) + 1, math.floor((rows - bh - 1) / 2) + 1
  scr:puts(ox, oy, "┌" .. string.rep("─", game.w * 2) .. "┐", st.border)
  for y = 1, game.h do
    scr:set(ox, oy + y, "│", st.border)
    scr:set(ox + bw - 1, oy + y, "│", st.border)
  end
  scr:puts(ox, oy + bh - 1, "└" .. string.rep("─", game.w * 2) .. "┘", st.border)
  for i = #game.body, 1, -1 do
    local c = game.body[i]
    scr:puts(ox + c[1] * 2 - 1, oy + c[2], i == 1 and "██" or "▓▓", i == 1 and st.head or st.body)
  end
  if game.food then scr:puts(ox + game.food[1] * 2 - 1, oy + game.food[2], "◖◗", st.food) end
  scr:puts(ox, oy + bh, ("Score %d   Best %d"):format(game.score, best), st.text)
  scr:puts(ox + 22, oy + bh, "arrows/wasd · p pause · r restart · q quit", st.dim)
  if not game.alive then scr:puts(ox + math.floor((bw - 17) / 2), oy + math.floor(bh / 2), " game over - r/q ", st.over)
  elseif paused then scr:puts(ox + math.floor((bw - 8) / 2), oy + math.floor(bh / 2), " paused ", st.text) end
  scr:flush()
end

local function interval() return math.max(0.05, 0.16 - game.score * 0.004) end

new_game()
draw()
local nextstep = now() + interval()
while true do
  local ev, a, b = ui.poll(math.max(0, nextstep - now()))
  if ev == "quit" then break
  elseif ev == "key" or ev == "char" then
    local name = a
    if ev == "key" and b and math.floor(b / 4) % 2 == 1 and name == "c" then break end
    if name == "q" or name == "escape" then break
    elseif name == "r" then new_game(); nextstep = now() + interval()
    elseif name == "p" and game.alive then paused = not paused
    elseif KEYS[name] and not paused then game:turn(KEYS[name]) end
    draw()
  elseif ev == "resize" then
    cols, rows = a, b
    scr:resize(cols, rows)
    new_game()
    draw()
  elseif ev == nil then
    if now() >= nextstep then
      if game.alive and not paused then
        game:step()
        if game.score > best then best = game.score end
      end
      nextstep = now() + interval()
      draw()
    end
  end
end
ui.close()
print(("Score %d"):format(game.score))
