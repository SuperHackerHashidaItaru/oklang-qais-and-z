var a = [0, 0, 0, 0]
var i = 0
while (i < 2000000) {
  a[0] = a[0] + 1
  i = i + 1
}
System.print(a[0])
