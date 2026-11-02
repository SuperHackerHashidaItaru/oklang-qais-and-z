class Cat {
  construct new() {}
  speak { 1 }
}
class Dog {
  construct new() {}
  speak { 2 }
}
var cat = Cat.new()
var dog = Dog.new()
var i = 0
var sum = 0
var flag = true
while (i < 5000000) {
  if (flag) {
    sum = sum + cat.speak
    flag = false
  } else {
    sum = sum + dog.speak
    flag = true
  }
  i = i + 1
}
System.print(sum)
