local m = {}
local i = 0
while i < 200000 do m["k" .. i] = i; i = i + 1 end
print(m["k0"] + m["k199999"])
