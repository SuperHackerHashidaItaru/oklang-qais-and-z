local i, left, right, flag = 0, 0, 0, false
while i < 5000000 do
  if flag then
    left = left + 1
    flag = false
  else
    right = right + 1
    flag = true
  end
  i = i + 1
end
print(left + right)
