# *args packing + iteration
def sum_all(*nums):
    total = 0
    for n in nums:
        total += n
    return total
acc = 0
i = 0
while i < 200_000:
    acc = acc + sum_all(i, i + 1, i + 2)
    i += 1
print(acc)
