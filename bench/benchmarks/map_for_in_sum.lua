local m = {k0 = 1, k1 = 2, k2 = 3, k3 = 4, k4 = 5, k5 = 6, k6 = 7, k7 = 8}
local pass, sum = 0, 0
while pass < 1000000 do
  for _, v in pairs(m) do sum = sum + v end
  pass = pass + 1
end
print(sum)
