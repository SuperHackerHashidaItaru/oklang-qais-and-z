def bump(value):
    return value + 1
value = 0
i = 0
while i < 1_000_000:
    value = bump(value)
    i = i + 1
print(value)
