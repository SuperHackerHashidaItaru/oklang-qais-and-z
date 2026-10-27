a = [1, 2, 3, 4, 5, 6, 7, 8]
pass_count = 0
sum_value = 0
while pass_count < 1_000_000:
    i = 0
    while i < 8:
        sum_value = sum_value + a[i]
        i = i + 1
    pass_count = pass_count + 1
print(sum_value)
