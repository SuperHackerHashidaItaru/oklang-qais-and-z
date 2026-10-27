i, left, right, flag = 0, 0, 0, False
while i < 5_000_000:
    if flag:
        left = left + 1
        flag = False
    else:
        right = right + 1
        flag = True
    i = i + 1
print(left + right)
