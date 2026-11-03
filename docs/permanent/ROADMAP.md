# oklang — roadmap v2 (2026-10-25, written the day the rewrite landed)

changes from the oct24 roadmap, per owner's review:
- pipeline isolation moved AFTER halloween (owner's call — the fresh game is
  one file; restart-on-change is fine for three days).
- the dorg3 migration pass is DROPPED entirely: dorg3 is a shipped game pinned
  to the old language; the halloween game is written fresh against the new
  features.
- debugger stays post-halloween, but **opcode printing (the disassembler) moves
  up** — it is the instrument the perf round needs first.
- everything else carried forward and re-sequenced. owner agrees with the shape
  modulo ordering; the order below is Z's proposal, owner vetoes freely.

## pre-halloween (final scope — language FREEZES EOD 27/10)

- **25th (today): tag v0.0.4 FIRST**, before any feature work. the working
  state is the release: register VM, error reporting with file:line:col +
  stack traces, the whole fix series, benchmark table in the notes. keep the
  ok-stack binary available for differential testing.
- **25th-26th: arrays** (oct24 spec + deltas: no slicing; natives len/push/pop
  now, .len/.push/.pop methods post-classes; fake ranges for for-in).
- **26th: numeric-for** (oct24 spec + one big delta: the int/float mode split
  is DROPPED — always f64 induction, step ±1 exact to 2^53, fractional steps
  documented as C-double behavior; 2 opcodes instead of 4). index-store
  discipline (single evaluation of base/index/value) lands inside arrays work.
- **27th: soak + freeze.** stretch: gc.pause/resume/collect/count if the game
  hitches on collection.
- **28th: buffer day** — absorb fallout, second e2e sweep with the
  aggressive+asan matrix.
- **29-31: game days.** no language work. missing something? fake it with
  arrays + closures.

## post-halloween ladder (Z's proposal)

**v0.0.5 — stabilization + instruments** ("make the tooling stop lying")
- e2e runner hardening: per-test timeout, failure buckets
  (compile-error/wrong-output/crash/timeout), asan/leak as first-class configs,
  don't let expected-error tests mask leaks.
- differential testing: every script under BOTH ok-stack and ok-register,
  outputs must match. this is what caught every real bug — make it permanent.
- commit the bench suite to the repo (bench/ + hyperfine + combine.py + these
  baselines; re-run per release, RSS tracked).
- parser/AST arena + error recovery (multiple errors per compile, sync to
  statement/expr boundary). for robustness and ownership clarity, not speed.
  also groundwork for LSP phase 2 and the tree-sitter cross-check corpus.
- exit-leak cleanup, warning cleanup.

**v0.0.6 — disassembler + codegen perf pass** ("instrument before surgery")
- okdebug.c: the real rewrite against okchunk.h contract comments. this is the
  owner's "support opcode instructions now" wish — and the tool that confirms
  the local_pressure/branch_toggle hypotheses (disassemble, count dispatches).
- compare+jump fusion (lua-style jump hint): kill boolean materialization in
  condition position.
- binop constant-operand folding: stop pre-materializing literals; use the
  existing p/q constant flags. likely most of the locals/arithmetic gap.
- jump threading + constant folding in the codegen pass (free from two-pass).
- re-bench. targets: locals <= 1.2x lua, branch <= 1.3x, loop at parity (with
  numeric-for), arithmetic <= 1.15x. calls/closures/fib already there.

**v0.0.7 — varargs + multi-return** (spec to write; design sketch in DECISIONS
D6 — register-window varargs, argc as a frame field, results at callee base =
zero copy, static result arity with destructuring).

**v0.0.8 — error model** (DECISIONS D7): errors as values, bytecode handler
frames (NO setjmp — wasm-safe), pcall now + try/catch sugar later, lazy
tracebacks, coro interplay settled. rides the error_pending machinery that
already exists from the callback fix.

**v0.0.9 — pipeline isolation + modules** (DECISIONS D8/D9): snapshot-swap
commit; then compile-time imports (`import x`, `import x as z`,
`from x import y`), run-once top-level, module registry keyed by interned
path, module exports as slot arrays (same index trick as globals), dynamic
import as the explicitly-dynamic escape hatch.
[update 2026-11-03: this wave is now 0.6.0 and much bigger — native module
loading (dlopen + ok_module.h), okproject.ok, the loader vtable, the CLI
build surface. the full draft with the M1-M14 rulings and the P1-P8
session-sized patch series is docs/drafts/MODULES.md; the import spelling
is now QUOTED paths (`import "std/io"`), superseding the plain form here.]
[second update, same day: the wave is FINAL and renumbered **0.8.0** — the
tidy lane (defer continuation, type/object, universal interning D20, the
coro C API) is v0.7.0. M1-M14 are ruled and closed, the capability
sandbox is designed in, and the patch series is re-densed to four
(OR-1..OR-4). the living draft is docs/drafts/MODULES.md, codename
"O Resolution".]

**v0.0.10 — maps v2** (oct24 spec stands: open addressing, linear probing,
parallel key/value arrays, backward-shift delete, no tombstones, splitmix64 +
FNV-1a cached in string header, tag mixed into hash). migrate globals_store
first. (flexible vs v0.0.9 — snapshot-cloning the old oktable works meanwhile;
if modules want it earlier, swap them.)

**v0.0.11 — UTF-8 strings + string stdlib** (oct24 design stands: codepoint
API, cached codepoint count, Go-style U+FFFD lossy decode, ascii idents v1).

**v0.1.0 — classes** (DECISIONS D10): frozen shape, copy-down single
inheritance, metaclass statics, primitives share one class object per type,
monomorphic inline caching, compiler-enforced definite init, best-effort
dtors, explicit close/defer for determinism.

**v0.1.x — coroutines**: stackful, lua-like; per-coro register file + call
stack; yield only from ok frames (native_call_depth guard — the counter
already exists). the resumer can be the C game loop (that direction was proven
safe by the array_foreach callback work).

**v0.2.0 — serialization + okc exe** (rides pipeline isolation's compile
result; same tagged-binary family) **+ debugger v0** (oct24 spec stands — line
tables, OP_BRK patching, run_until stepping) **+ incremental GC** (slice
budgeting; kill the spikes properly; array stores get write barriers then).

**v0.2.x — LSP phase 1** (thin server wrapping tree-sitter-oklang:
highlight/fold/symbols/diagnostics-lite — zero compiler changes) **+ the tiny
event loop lib** (DECISIONS D12: epoll/kqueue/select + wasm rAF backend,
min-heap timers, prepare/check/idle hooks; drives coros; fixes the web fps
wobble as a side effect).

**v1.0** — stdlib complete (DECISIONS D16), LSP phase 3 (semantic: check-mode
compiler + symbol tables; the compile-time-globals doctrine makes go-to-def
trivial), engine milestone: the 2D app engine running games, apps, and — per
the pledge — the portfolio site's backend.

### ordering rationale (argue with any of it)
- disassembler before perf work: never optimize blind; it is also the cheapest
  living documentation of the encoding.
- error model before classes: dtor/finally semantics need the unwind story.
- pipeline isolation before modules: modules ARE the pipeline's client.
- classes before coros: bigger semantic surface first; coros are mechanical
  once frames are per-coro. (swap freely if motivation says otherwise.)
- LSP phase 1 can start any time — it's independent of the compiler.

## explicitly NOT in v1
JIT. threads. int scalars (revisit only as typed array buffers for engine
assets — DECISIONS D3). regex. package registry. windows IOCP-first event
loop (select first, windows-later).

## parking lot (open questions, no home yet)
mixed-type `==` (error vs false). implicit `self` resolution order in classes.
REPL locals persistence (wrap each line in glob scope?). the register-file
UINT16_MAX clamp. final fate of C-style for (Z says freeze-not-drop). ordered
maps (v2, post-v1). hot-reload semantics for a live game (v2 of pipeline
isolation).
