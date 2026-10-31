m = {}
i = 0
while i < 200_000:
    m[f"k{i}"] = i
    i = i + 1
print(m["k0"] + m["k199999"])
