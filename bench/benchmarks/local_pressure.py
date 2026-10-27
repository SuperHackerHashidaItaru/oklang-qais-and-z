i = 0
a = b = c = d = e = f = g = h = 0
while i < 1_000_000:
    a = a + 1; b = b + 2; c = c + 3; d = d + 4
    e = e + 5; f = f + 6; g = g + 7; h = h + 8
    i = i + 1
print(a + b + c + d + e + f + g + h)
