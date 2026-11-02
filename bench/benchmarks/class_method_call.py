class Node:
    def bump(self, x):
        return x + 1

n = Node()
i = 0
r = 0
while i < 5_000_000:
    r = n.bump(r)
    i = i + 1
print(r)
