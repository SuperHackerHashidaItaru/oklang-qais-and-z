var makeCounter = Fn.new {
  var captured = 0
  return Fn.new {
    captured = captured + 1
    return captured
  }
}
var next = makeCounter.call()
var i = 0
var result = 0
while (i < 500000) {
  result = next.call()
  i = i + 1
}
System.print(result)
