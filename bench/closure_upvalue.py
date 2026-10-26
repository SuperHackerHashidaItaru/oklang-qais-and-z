def make_counter():
    captured = 0
    def next_value():
        nonlocal captured
        captured = captured + 1
        return captured
    return next_value
next_value = make_counter()
i = result = 0
while i < 500_000:
    result = next_value()
    i = i + 1
print(result)
