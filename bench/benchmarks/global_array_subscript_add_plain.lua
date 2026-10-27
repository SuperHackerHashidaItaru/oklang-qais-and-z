g = {0, 0, 0, 0} -- global container
local i = 0
while i < 2000000 do
  g[1] = g[1] + 1
  i = i + 1
end
print(g[1])
