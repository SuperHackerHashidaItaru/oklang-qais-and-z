local a = {0, 0, 0, 0}
local i = 3 -- corresponding element for Lua's 1-based tables
local n = 0
while n < 2000000 do
  a[i] = a[i] + 1 -- Lua has no compound-assignment syntax
  n = n + 1
end
print(a[i])
