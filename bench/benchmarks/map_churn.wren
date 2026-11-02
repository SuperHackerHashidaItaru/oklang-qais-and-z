var m = {}
var i = 0
while (i < 200000) {
  m["k" + i.toString] = i
  if (i % 2 == 0) {
    m.remove("k" + i.toString)
  }
  i = i + 1
}
System.print(m["k199999"])
