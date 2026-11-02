local Point = {}
Point.__index = Point
function Point.new(x, y)
  return setmetatable({x = x, y = y}, Point)
end
function Point:sum()
  return self.x + self.y
end
local i = 0
local s = 0
while i < 2000000 do
  local p = Point.new(i, i)
  s = s + p:sum()
  i = i + 1
end
print(s)
