var i = 0
var left = 0
var right = 0
var flag = false
while (i < 5000000) {
  if (flag) {
    left = left + 1
    flag = false
  } else {
    right = right + 1
    flag = true
  }
  i = i + 1
}
System.print(left + right)
