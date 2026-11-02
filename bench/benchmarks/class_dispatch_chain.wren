class Base {
  construct new() {}
  tag { 7 }
}
class Mid is Base {
  construct new() { super() }
}
class Leaf is Mid {
  construct new() { super() }
}
var leaf = Leaf.new()
var i = 0
var sum = 0
while (i < 5000000) {
  sum = sum + leaf.tag
  i = i + 1
}
System.print(sum)
