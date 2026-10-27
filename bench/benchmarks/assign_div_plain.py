factor = 1.0000001
x = 1
i = 0
while i < 2000000:
    x = x / factor
    i = i + 1
print(x)
