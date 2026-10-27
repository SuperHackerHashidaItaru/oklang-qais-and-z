i = 0
sum_value = 0
while i < 200_000:
    a = [i, i + 1, i + 2, i + 3]
    sum_value = sum_value + a[0] + a[3]
    i = i + 1
print(sum_value)
