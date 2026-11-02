class Hot {
  construct new() {
    _a = 0
    _b = 0
    _c = 0
    _d = 0
  }
  run(n) {
    var i = 0
    while (i < n) {
      _a = _a + 1
      _b = _b + 2
      _c = _c + 3
      _d = _d + 4
      i = i + 1
    }
    return _a + _b + _c + _d
  }
}
System.print(Hot.new().run(5000000))
