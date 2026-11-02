var m = {"k0": 1, "k1": 2, "k2": 3, "k3": 4, "k4": 5, "k5": 6, "k6": 7, "k7": 8}
var pass = 0
var sum = 0
while (pass < 1000000) {
  sum = sum + m["k0"] + m["k1"] + m["k2"] + m["k3"] + m["k4"] + m["k5"] + m["k6"] + m["k7"]
  pass = pass + 1
}
System.print(sum)
