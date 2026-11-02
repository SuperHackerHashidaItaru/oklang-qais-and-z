class Vec2 {
  construct new(x, y) {
    _x = x
    _y = y
  }
  x { _x }
  y { _y }
  + (o) {
    return Vec2.new(_x + o.x, _y + o.y)
  }
}
var a = Vec2.new(1, 2)
var d = Vec2.new(1, 1)
var i = 0
while (i < 1000000) {
  a = a + d
  i = i + 1
}
System.print(a.x + a.y)
