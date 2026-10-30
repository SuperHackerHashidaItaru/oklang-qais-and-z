# oklang — roadmap to Halloween (written 2026-10-24, ~22:30)

Deadline: game playable by 31/10. Budget: **3 days of pure game time (29/30/31) +
buffer day (28)**. Everything below is sequenced so the language is FROZEN at
end of 27/10 — after that only bugfixes, no features, no syntax changes.

## where we actually are

- register VM: 109/109 e2e, clean under plain / OK_AGGRESSIVE_GC / ASan.
- dorg3 boots on ok again (see `dorg3-boot-fix.patch`): root cause was a GC
  rooting window, not the compiler — details in the patch header.
- runtime errors now carry: named callee, named operator, file:line:col and a
  stack trace (this round). This is the foundation for both the debugger and
  pcall.
- perf vs target: 376ms vs lua 81.6ms on the 10M loop (4.6x). python3 is
  2135ms — we are comfortably in "real VM" territory already.

## the week

### tonight / 25th — stabilize + first feature
1. apply `dorg3-boot-fix.patch`, run dorg3 on the real machine, commit.
2. **numeric-for** (`spec 027`): new ops, tight dispatch. keep C-style `for`
   parsing until after Halloween (deprecate with a warning); hard-drop later.
   this is the single biggest perf lever we have (analysis below).
3. **arrays** (`spec 026`): literal, index get/set, push/pop, len, for-in.
   this unblocks "structs via arrays + closures" for game code.

### 26th — compile pipeline isolation (`spec: pipeline-isolation`)
- compiler writes to staging stores; commit atomically on success. one broken
  TU can never mutate live globals/functions again.
- VM stops hoarding the source: parse -> compile -> release; keep only the
  interned path + line tables for errors/debug. unblocks multi-file later.
- if time: varargs (`...` packs into an array — now that arrays exist).

### 27th — buffer + freeze
- land the game on new ok (it will use arrays + numeric-for heavily).
- stretch: `gc.pause()/gc.resume()/gc.collect()/gc.count()` (trivial with the
  current gc; the game wants pause-around-frames and collect-between-screens).
- **EOD: language freeze.** tag it. only bugfixes after this point.

### 28th — buffer / polish
- absorb fallout, perf pass if anything is hot, second e2e sweep with the
  aggressive+asan matrix in CI shape.

### 29–31 — game days
- no language work. if something is missing, fake it with arrays + closures.

## after halloween (priority order)
1. maps v2 (open addressing — full spec below, replaces oktable).
2. error model + pcall (errors as values, setjmp-based unwind).
3. coroutines (stackful, per-frame anim use case — design below).
4. okc exe (compile-to-bytecode; vehicle for module caching + shipping without
   source) + serialization (same tagged-binary format family).
5. debugger v0 (`spec: debugger-v0` — line tables already exist and are
   proven by this round's stack traces).
6. classes (informed by a month of arrays+closures patterns).
7. utf8 zero-dep treatment, test-runner passes, warning cleanup, exit-leaks.

## design answers (the open questions from this round)

### maps — what I suggest
Full rewrite, **open addressing with linear probing** (replaces
buckets-of-arrays):
- capacity = power of two, slot = `hash & (cap-1)`, load factor 0.7, grow x2.
- **parallel arrays** `keys[]` / `values[]` (probe touches keys only — cache
  friendly; values stream in once a hit is found).
- hash: **splitmix64 for numbers**, **FNV-1a for strings with the hash cached
  in the string header** (computed once at intern). no external deps, ~15
  lines total. (wyhash later if we ever care.)
- **deletion by backward-shift** (no tombstones): ~15 lines, keeps probes
  pure — tombstone buildup is the classic silent perf rot.
- hash the tag into the value hash so `1`, `true`, `"1"` never collide into
  equality shortcuts.
- NOT robin hood (complexity not worth it at our scale), NOT swiss (needs
  SIMD intrinsics to shine; revisit post-classes).
- no Lua-style array part (real arrays exist), no insertion order (v1;
  python-style ordered maps are an upgrade path via entry-index table).
Estimated ~300 lines replacing oktable.

### coroutines — the limitation, precisely
You understood it right: **you can never yield "through" a C frame.** A ok
coroutine that calls a native (C) function cannot yield from inside it — the
C stack frame cannot be suspended. Concretely: `array_foreach(a, fu(x) ?
coro.yield(x); ...)` is illegal — the yield happens while C (foreach) is on
the stack. Your use case — a coro per animation that yields once per frame
from **pure ok code** — is 100% supported by the standard design: each coro
gets its own ok register file + call stack, `resume` switches in, `yield`
switches back to the resumer (the C game loop calling resume is fine — that's
C -> ok -> back, never ok -> C -> ok). We enforce it with a runtime guard:
`native_call_depth > 0` on yield = clean runtime error "yield across native
boundary" (that counter already exists — we added it for ok_native_error).
Design: stackful, context switch via ucontext or a small asm trampoline
(x86_64 + aarch64 only, ~100 lines, zero deps).

### utf8 — zero deps, python3-ish
Strings stay UTF-8 byte arrays. `len` = codepoint count (cached in the string
header — strings are immutable). Indexing/slicing by codepoint (O(n) scan v1,
we can cache an offset index later). Iteration yields codepoints. Decoding is
a hand-rolled ~40-line DFA (Go-style: invalid byte -> U+FFFD, advance 1, never
crash, never loop). Identifiers stay ASCII for now (XID later). Stdlib:
`utf8.len/char/codepoint/valid`. No ICU, no tables — python3 got away with
self-contained unicode for 20 years; we can too (full normalization tables
are a "when someone needs it" problem).

### benchmarks + numeric-for
The 10M loop today compiles to roughly: load i, load 1, add, store i, load i,
load const, lt, jump — ~7 dispatches per iteration, every operand a tagged
value check. Lua's `FORLOOP` is one fused op with an integer fast path
(Lua 5.4 has a real integer subtype). That's most of the 4.6x gap right
there; the rest is value tagging (we pay strict-tag checks everywhere after
the GC double-aliasing fix).
numeric-for targets ~2 fused ops/iter (FORPREP validates + pre-computes the
trip count; FORLOOP increments, compares, branches). Realistic landing zone:
**~150-200ms** on this bench (2-2.5x off lua). Further than that needs
NaN-boxing with a real pointer-tag scheme (design note in PORT_README) and
superinstructions — post-Halloween.

Also worth 10 minutes someday: `+=` sugar (the bench's `sum = sum + i` is
wordy), constant-folding `i + 1`.
