a = []
i = 0
while i < 200_000:
    a.append(i)
    i = i + 1
print(len(a) + a[199_999])
