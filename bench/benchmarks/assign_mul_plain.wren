var factor = 1.0000001
var x = 1
var i = 0
while (i < 2000000) {
  x = x * factor
  i = i + 1
}
System.print(x)
