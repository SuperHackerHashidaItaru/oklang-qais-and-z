local Cat = {}
Cat.__index = Cat
function Cat.new()
  return setmetatable({}, Cat)
end
function Cat:speak()
  return 1
end
local Dog = {}
Dog.__index = Dog
function Dog.new()
  return setmetatable({}, Dog)
end
function Dog:speak()
  return 2
end
local cat = Cat.new()
local dog = Dog.new()
local i = 0
local sum = 0
local flag = true
while i < 5000000 do
  if flag then
    sum = sum + cat:speak()
    flag = false
  else
    sum = sum + dog:speak()
    flag = true
  end
  i = i + 1
end
print(sum)
