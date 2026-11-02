var i = 0
var sum = 0
while (i < 200000) {
  var a = [i, i + 1, i + 2, i + 3]
  sum = sum + a[0] + a[3]
  i = i + 1
}
System.print(sum)
