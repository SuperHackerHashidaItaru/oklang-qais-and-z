class Point {
  construct new(x, y) {
    _x = x
    _y = y
  }
  sum { _x + _y }
}
var i = 0
var s = 0
while (i < 2000000) {
  var p = Point.new(i, i)
  s = s + p.sum
  i = i + 1
}
System.print(s)
