-- Snake rules, no I/O: a grid, a snake, food, a step function. Deterministic with an injected random function (tests/snake.lua).
local M = {}
M.__index = M

local DIRS = {up = {0, -1}, down = {0, 1}, left = {-1, 0}, right = {1, 0}}

-- rand(n) -> integer in 1..n
function M.new(w, h, rand)
  local self = setmetatable({w = w, h = h, rand = rand or function(n) return math.random(n) end, score = 0, alive = true}, M)
  local cx, cy = math.floor(w / 2), math.floor(h / 2)
  self.body = {{cx, cy}, {cx - 1, cy}, {cx - 2, cy}} -- head first
  self.dir, self.pending = "right", "right"
  self.occupied = {}
  for _, c in ipairs(self.body) do self.occupied[c[2] * w + c[1]] = true end
  self:place_food()
  return self
end

function M:place_food()
  local free = self.w * self.h - #self.body
  if free <= 0 then self.food = nil; return end
  local n = self.rand(free)
  for y = 1, self.h do
    for x = 1, self.w do
      if not self.occupied[y * self.w + x] then
        n = n - 1
        if n == 0 then self.food = {x, y}; return end
      end
    end
  end
end

-- a turn takes effect on the next step; reversing straight into the neck is ignored
function M:turn(dir)
  local d, cur = DIRS[dir], DIRS[self.dir]
  if d and not (d[1] == -cur[1] and d[2] == -cur[2]) then self.pending = dir end
end

-- returns "moved", "ate" or "dead"
function M:step()
  if not self.alive then return "dead" end
  self.dir = self.pending
  local d, head = DIRS[self.dir], self.body[1]
  local nx, ny = head[1] + d[1], head[2] + d[2]
  local eating = self.food ~= nil and nx == self.food[1] and ny == self.food[2]
  local tail = self.body[#self.body]
  if not eating then self.occupied[tail[2] * self.w + tail[1]] = nil end -- the tail cell is free when the head arrives
  if nx < 1 or ny < 1 or nx > self.w or ny > self.h or self.occupied[ny * self.w + nx] then
    if not eating then self.occupied[tail[2] * self.w + tail[1]] = true end
    self.alive = false
    return "dead"
  end
  table.insert(self.body, 1, {nx, ny})
  self.occupied[ny * self.w + nx] = true
  if eating then
    self.score = self.score + 1
    self:place_food()
    return "ate"
  end
  self.body[#self.body] = nil
  return "moved"
end

return M
