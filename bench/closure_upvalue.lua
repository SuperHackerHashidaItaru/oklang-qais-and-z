local function make_counter()
  local captured = 0
  return function() captured = captured + 1; return captured end
end
local next_value = make_counter()
local i, result = 0, 0
while i < 500000 do result = next_value(); i = i + 1 end
print(result)
