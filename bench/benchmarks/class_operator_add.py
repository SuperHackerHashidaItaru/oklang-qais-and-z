class Vec2:
    def __init__(self, x, y):
        self.x = x
        self.y = y

    def __add__(self, o):
        return Vec2(self.x + o.x, self.y + o.y)

a = Vec2(1, 2)
d = Vec2(1, 1)
i = 0
while i < 1_000_000:
    a = a + d
    i = i + 1
print(a.x + a.y)
