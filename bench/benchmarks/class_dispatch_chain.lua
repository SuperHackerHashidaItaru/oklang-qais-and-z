local Base = {}
Base.__index = Base
function Base.new()
  return setmetatable({}, Base)
end
function Base:tag()
  return 7
end
local Mid = setmetatable({}, {__index = Base})
Mid.__index = Mid
function Mid.new()
  return setmetatable({}, Mid)
end
local Leaf = setmetatable({}, {__index = Mid})
Leaf.__index = Leaf
function Leaf.new()
  return setmetatable({}, Leaf)
end
local leaf = Leaf.new()
local i = 0
local sum = 0
while i < 5000000 do
  sum = sum + leaf:tag()
  i = i + 1
end
print(sum)
