local a = {1, 2, 3, 4, 5, 6, 7, 8}
local pass, sum = 0, 0
while pass < 1000000 do
  for _, v in ipairs(a) do sum = sum + v end
  pass = pass + 1
end
print(sum)
