local Node = {}
Node.__index = Node
function Node.new()
  return setmetatable({}, Node)
end
function Node:bump(x)
  return x + 1
end
local n = Node.new()
local i = 0
local r = 0
while i < 5000000 do
  r = n:bump(r)
  i = i + 1
end
print(r)
