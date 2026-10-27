a = [0, 0, 0, 0]
i = 2  # local index variable; held fixed to isolate the register-index path
n = 0
while n < 2_000_000:
    a[i] += 1
    n = n + 1
print(a[i])
