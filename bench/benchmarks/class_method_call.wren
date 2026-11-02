class Node {
  construct new() {}
  bump(x) {
    return x + 1
  }
}
var n = Node.new()
var i = 0
var r = 0
while (i < 5000000) {
  r = n.bump(r)
  i = i + 1
}
System.print(r)
