var m = {"k0": 1, "k1": 2, "k2": 3, "k3": 4, "k4": 5, "k5": 6, "k6": 7, "k7": 8}
var pass = 0
var sum = 0
while (pass < 1000000) {
  for (k in m.keys) {
    sum = sum + m[k]
  }
  pass = pass + 1
}
System.print(sum)
