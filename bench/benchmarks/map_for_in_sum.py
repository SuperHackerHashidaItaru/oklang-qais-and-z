m = {"k0": 1, "k1": 2, "k2": 3, "k3": 4, "k4": 5, "k5": 6, "k6": 7, "k7": 8}
pass_count = 0
sum_value = 0
while pass_count < 1_000_000:
    for key in m:
        sum_value = sum_value + m[key]
    pass_count = pass_count + 1
print(sum_value)
