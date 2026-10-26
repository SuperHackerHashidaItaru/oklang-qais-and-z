g = [0, 0, 0, 0]  # module-global container
i = 0
while i < 2_000_000:
    g[0] = g[0] + 1
    i = i + 1
print(g[0])
