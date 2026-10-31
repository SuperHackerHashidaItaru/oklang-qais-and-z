m = {"k0": 1, "k1": 2, "k2": 3, "k3": 4, "k4": 5, "k5": 6, "k6": 7, "k7": 8}
pass_count = 0
sum_value = 0
while pass_count < 1_000_000:
    sum_value = sum_value + m["k0"] + m["k1"] + m["k2"] + m["k3"] + m["k4"] + m["k5"] + m["k6"] + m["k7"]
    pass_count = pass_count + 1
print(sum_value)
