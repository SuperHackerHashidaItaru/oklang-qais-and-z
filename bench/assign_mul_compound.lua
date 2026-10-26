local factor = 1.0000001
local x = 1
local i = 0
while i < 2000000 do
  x = x * factor
  i = i + 1
end
print(x)
