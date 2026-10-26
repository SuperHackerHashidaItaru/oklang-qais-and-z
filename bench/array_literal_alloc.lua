local i, sum = 0, 0
while i < 200000 do
  local a = {i, i + 1, i + 2, i + 3}
  sum = sum + a[1] + a[4]
  i = i + 1
end
print(sum)
