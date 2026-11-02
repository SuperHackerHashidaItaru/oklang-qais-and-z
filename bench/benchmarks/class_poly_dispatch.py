class Cat:
    def speak(self):
        return 1

class Dog:
    def speak(self):
        return 2

cat = Cat()
dog = Dog()
i = 0
sum = 0
flag = True
while i < 5_000_000:
    if flag:
        sum = sum + cat.speak()
        flag = False
    else:
        sum = sum + dog.speak()
        flag = True
    i = i + 1
print(sum)
