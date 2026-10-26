local a = {}
local i = 0
while i < 200000 do a[#a + 1] = i; i = i + 1 end
print(#a + a[200000])
