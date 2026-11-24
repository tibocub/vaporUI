-- snake rules (examples/snake/game.lua): movement, growth, walls, self collision, no reversing, food placement
package.path = "examples/snake/?.lua;" .. package.path
local Game = require("game")
local seq = 0
local function rand(n) seq = seq + 1; return (seq - 1) % n + 1 end

local g = Game.new(10, 8, rand)
assert(#g.body == 3 and g.dir == "right" and g.alive)
local hx, hy = g.body[1][1], g.body[1][2]
assert(g:step() == "moved" or g:step() == "ate")
assert(g.body[1][1] == hx + 1 and g.body[1][2] == hy)

-- reversing into the neck is ignored
g = Game.new(10, 8, rand); g.food = {1, 1}
g:turn("left"); g:step()
assert(g.body[1][1] == 6 and g.alive, "reverse ignored")
g:turn("up"); g:step(); assert(g.body[1][2] == 3, "turned up")

-- eating grows by one and scores; food moves to a free cell
g = Game.new(10, 8, rand)
g.food = {g.body[1][1] + 1, g.body[1][2]}
assert(g:step() == "ate" and #g.body == 4 and g.score == 1)
assert(g.food and not g.occupied[g.food[2] * g.w + g.food[1]], "food on a free cell")

-- walls kill
g = Game.new(5, 5, rand); g.food = {1, 1}
for _ = 1, 10 do if g:step() == "dead" then break end end
assert(not g.alive and g:step() == "dead")

-- running into itself kills
g = Game.new(10, 10, rand)
g.body = {{5, 5}, {4, 5}, {4, 4}, {5, 4}, {6, 4}, {6, 5}, {6, 6}}; g.occupied = {}
for _, c in ipairs(g.body) do g.occupied[c[2] * g.w + c[1]] = true end
g.dir, g.pending, g.food = "up", "up", {1, 1}
assert(g:step() == "dead", "head into body")

-- following the tail is allowed (the tail cell is free when the head arrives)
g = Game.new(10, 10, rand)
g.body = {{5, 5}, {5, 6}, {4, 6}, {4, 5}}; g.occupied = {}
for _, c in ipairs(g.body) do g.occupied[c[2] * g.w + c[1]] = true end
g.dir, g.pending, g.food = "up", "up", {1, 1}
g:turn("left"); assert(g:step() == "moved" and g.alive, "moving into the vacated tail cell")

-- a full board leaves no food
g = Game.new(3, 1, rand); g.body = {{3, 1}, {2, 1}}; g.occupied = {[1 * 3 + 3] = true, [1 * 3 + 2] = true}
g:place_food(); assert(g.food and g.food[1] == 1)
g.body[3] = {1, 1}; g.occupied[1 * 3 + 1] = true; g:place_food(); assert(g.food == nil)
print("ok")
