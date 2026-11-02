class Base:
    def tag(self):
        return 7

class Mid(Base):
    pass

class Leaf(Mid):
    pass

leaf = Leaf()
i = 0
sum = 0
while i < 5_000_000:
    sum = sum + leaf.tag()
    i = i + 1
print(sum)
