-- varargs packing + iteration
local function sum_all(...)
  local total = 0
  for _, n in ipairs({...}) do
    total = total + n
  end
  return total
end
local acc, i = 0, 0
while i < 200000 do
  acc = acc + sum_all(i, i + 1, i + 2)
  i = i + 1
end
print(acc)
