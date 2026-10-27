local function bump(value) return value + 1 end
local value, i = 0, 0
while i < 1000000 do
  value = bump(value)
  i = i + 1
end
print(value)
