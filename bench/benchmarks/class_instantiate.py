class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y

    def sum(self):
        return self.x + self.y

i = 0
s = 0
while i < 2_000_000:
    p = Point(i, i)
    s = s + p.sum()
    i = i + 1
print(s)
