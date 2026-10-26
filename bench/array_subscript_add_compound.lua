local a = {0, 0, 0, 0}
local i = 0
while i < 2000000 do
  a[1] = a[1] + 1
  i = i + 1
end
print(a[1])
