local m = {}
local i = 0
while i < 200000 do
  m["k" .. i] = i
  if i % 2 == 0 then m["k" .. i] = nil end
  i = i + 1
end
print(m["k199999"])
