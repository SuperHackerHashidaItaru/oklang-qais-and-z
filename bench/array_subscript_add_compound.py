a = [0, 0, 0, 0]
i = 0
while i < 2_000_000:
    a[0] += 1
    i = i + 1
print(a[0])
