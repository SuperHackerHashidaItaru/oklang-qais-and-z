m = {}
i = 0
while i < 200_000:
    m[f"k{i}"] = i
    if i % 2 == 0:
        del m[f"k{i}"]
    i = i + 1
print(m["k199999"])
