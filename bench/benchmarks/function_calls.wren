class Bump {
  static bump(value) {
    return value + 1
  }
}
var value = 0
var i = 0
while (i < 1000000) {
  value = Bump.bump(value)
  i = i + 1
}
System.print(value)
