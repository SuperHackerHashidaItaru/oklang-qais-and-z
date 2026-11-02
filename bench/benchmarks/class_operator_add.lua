local Vec2 = {}
Vec2.__index = Vec2
Vec2.__add = function(l, r)
  return setmetatable({x = l.x + r.x, y = l.y + r.y}, Vec2)
end
local function new(x, y)
  return setmetatable({x = x, y = y}, Vec2)
end
local a = new(1, 2)
local d = new(1, 1)
local i = 0
while i < 1000000 do
  a = a + d
  i = i + 1
end
print(a.x + a.y)
