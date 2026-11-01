-- multi-return + destructuring in a loop
local function pair(a, b) return b, a end
local acc, i = 0, 0
while i < 1000000 do
  local x, y = pair(i, i + 1)
  acc = acc + x - y
  i = i + 1
end
print(acc)
