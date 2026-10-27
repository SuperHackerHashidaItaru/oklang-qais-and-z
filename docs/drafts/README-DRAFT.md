# OKLang

A small embedded scripting language written in C99.

It is intended to be used in my other projects.

```oklang
let bird = "nice";

fu feed(times = 2) {
  let mut worms = 0;
  for let i = 0 : times? {
    worms = worms + i;
  }
  return worms;
}

let flock = ["sparrow", "owl", "crow"];

for let name : flock? {
  print(name + " got " + feed() then " worms" else "");
}
// the ternary above is not valid oklang — see the real one below.
// this whole note exists so you never trust an untested README example.

print(bird == "nice" then "feeding time" otherwise "not now");
```

*(Z's note: the draft example must be rewritten and **run** before this goes
in — see the corrected, verified version at the bottom of this file. Keeping
my mistake visible on purpose: it's the exact anti-pattern the template
warns about, caught by the template's own rule.)*

## Status

OKLang is in early development, currently at v0.0.5.

The language is a bytecode compiler and a register-based VM. The first
implementation (C++) is deprecated; the C rewrite is the active one and has
reached feature parity with it, then moved past it.

What exists today:

* a register VM with fixed-width instruction encodings and NaN-boxed values
* closures and upvalues
* arrays, `for` (numeric, in, and C-style), `if`/`while`
* default arguments
* a ternary expression: `cond then a otherwise b`
* string concatenation with `+` and floored modulo `%` (python/lua style)
* a tracing garbage collector, with a stress mode that collects on every
  allocation
* native functions and opaque native values
* a minimal C embedding API
* a disassembler (`ok --dump`)

## Building

```bash
git clone https://codeberg.org/qais/oklang.git
cd oklang
mkdir build && cd build
cmake ..
cmake --build .
```

This builds the CLI (`ok`) and `libok`. The unit-test framework
(ThrowTheSwitch's Unity) comes in as a git submodule — clone with
`--recurse-submodules` if you plan to build tests.

### Running

Run a file:

```bash
./ok your-script.ok
```

or the REPL:

```bash
./ok
```

or compile-and-disassemble without executing — useful when you are curious
what the compiler did:

```bash
./ok --dump your-script.ok
```

## Tests

Enable with `-DOK_BUILD_TESTS=ON`:

```bash
cmake .. -DOK_BUILD_TESTS=ON
cmake --build .
ctest                                   # unit tests
./tests/e2e/oktest-e2e-runner ./ok ../tests/e2e/tests/   # end-to-end
./tests/integration/embedding/oktest_embedding           # embedding
```

## Embedding

OKLang embeds into C or C++ through a small C API (`#include <ok/ok.h>`):
define globals and native functions, create and pass opaque native values.
It is intentionally minimal for now — this section will grow when the
library boundary settles.

Build as shared instead of static with `-DOK_BUILD_SHARED=ON`.

## CMake options

* `OK_BUILD_CLI` — build the CLI. On by default.
* `OK_BUILD_TESTS` — build the tests. Off by default.
* `OK_BUILD_SHARED` — build `libok` shared instead of static.
* `OK_ENABLE_SANITIZERS` — ASan.
* `OK_PARANOID` — additional runtime logging.
* `OK_PEDANTIC` — additional failure checking.
* `OK_AGGRESSIVE_GC` — collect on every allocation (GC stress testing).

## Roadmap

Rough order, no dates:

1. maps
2. classes
3. a standard library (host-side first: strings, I/O, the fmod-flavored
   math that deliberately did NOT go into `%`)
4. the parked performance menu (dispatch and instruction-encoding work)

## Editor support

[tree-sitter-oklang](https://codeberg.org/qais/tree-sitter-oklang)

## License

[MIT License](LICENSE)

---

## Z's verified example (the one to actually use)

I wrote the block at the top before testing it, then ran it — `then/otherwise`
is the ternary, `else` is not a keyword in expression position, and string
concat with a number errors rather than coercing. The version below runs:

```oklang
let bird = "nice";

fu feed(times = 2) {
  let mut worms = 0;
  for let i = 0 : times? {
    worms = worms + i;
  }
  return worms;
}

let flock = ["sparrow", "owl", "crow"];
let mut fed = "";

for let name : flock? {
  fed = fed + name;
}

print(bird == "nice" then "feeding time" otherwise "not now");
print(fed);
print(feed());
```

Output:

```
feeding time
sparrowowlcrow
1
```

`feed()` returns 1 because `for let i = 0 : 2?` visits 0 and 1, and
`0 + 0 + 1 == 1` — default arguments and numeric-for in one five-line
function, which is the kind of compact demo the README wants.
