var a = [1, 2, 3, 4, 5, 6, 7, 8]
var pass = 0
var sum = 0
while (pass < 1000000) {
  var i = 0
  while (i < 8) {
    sum = sum + a[i]
    i = i + 1
  }
  pass = pass + 1
}
System.print(sum)
