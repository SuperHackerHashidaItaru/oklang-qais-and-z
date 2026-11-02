var a = [0, 0, 0, 0]
var i = 2
var n = 0
while (n < 2000000) {
  a[i] = a[i] + 1
  n = n + 1
}
System.print(a[i])
