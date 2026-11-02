local Hot = {}
Hot.__index = Hot
function Hot.new()
  return setmetatable({a = 0, b = 0, c = 0, d = 0}, Hot)
end
function Hot:run(n)
  local i = 0
  while i < n do
    self.a = self.a + 1
    self.b = self.b + 2
    self.c = self.c + 3
    self.d = self.d + 4
    i = i + 1
  end
  return self.a + self.b + self.c + self.d
end
print(Hot.new():run(5000000))
