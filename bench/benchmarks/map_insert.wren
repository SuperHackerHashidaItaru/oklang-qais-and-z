var m = {}
var i = 0
while (i < 200000) {
  m["k" + i.toString] = i
  i = i + 1
}
System.print(m["k0"] + m["k199999"])
