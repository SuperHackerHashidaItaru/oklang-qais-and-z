class Hot:
    def __init__(self):
        self.a = 0
        self.b = 0
        self.c = 0
        self.d = 0

    def run(self, n):
        i = 0
        while i < n:
            self.a = self.a + 1
            self.b = self.b + 2
            self.c = self.c + 3
            self.d = self.d + 4
            i = i + 1
        return self.a + self.b + self.c + self.d

print(Hot().run(5_000_000))
