# tuple-return + unpacking in a loop
def pair(a, b):
    return b, a
acc = 0
i = 0
while i < 1_000_000:
    x, y = pair(i, i + 1)
    acc = acc + x - y
    i += 1
print(acc)
