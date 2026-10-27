local a = {1, 2, 3, 4, 5, 6, 7, 8}
local pass, sum = 0, 0
while pass < 1000000 do
  local i = 1
  while i <= 8 do sum = sum + a[i]; i = i + 1 end
  pass = pass + 1
end
print(sum)
