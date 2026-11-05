# delta blue — oklang vs wren vs python3 (2026-11-03, sandbox)

the benchmark: DeltaBlue, the classic incremental constraint-hierarchy solver
(Smalltalk -> JS -> Dart lineage, as shipped in wren's repo as
`test/benchmark/delta_blue.{wren,py}`). wren's repo carries wren + python
versions; this round adds the oklang port (`benchmarks/delta_blue.ok`) and the
comparison runner (`bench/delta_blue_bench.py`).

the ask (Q's words): "create a delta blue bench for oklang (wren and python
already have one in wren's repo so we can borrow it and make our own and
compare)".

## methodology

- external wall-clock (perf_counter around the whole process), 2 warmup + 10
  timed runs, median reported. totals cross-checked every run: all three
  print 14065400 (oklang prints `%g`: 1.40654e+07 — numerically equal, the
  runner asserts it).
- same iteration counts as the wren reference: 40 x (chain(100) +
  projection(100)).
- sandbox: gcc 14.2, oklang release (-O2) and release+threaded-dispatch;
  wren_cli (release build); python 3.12.16. re-run on the owner's machine
  before quoting numbers anywhere public — sandbox absolute values wobble
  run to run (~3%), the RATIOS were stable across sessions.

## results

| engine | median | min | total |
|---|---:|---:|---|
| ok (release, -O2) | 249.7 ms | 249.1 | 1.40654e+07 |
| ok (release, threaded dispatch) | 243.3 ms | 242.2 | 1.40654e+07 |
| wren_cli | 129.5 ms | 128.8 | 14065400 |
| python3 | 201.9 ms | 201.3 | 14065400 |

relative to wren: **ok 1.93x slower, ok-threaded 1.88x, python 1.56x**.
in-language timers agree (wren System.clock 0.126s, python process_time
0.187s) — startup is noise at this scale.

## the honest read

- **where this sits against our own numbers**: PERF-PASS-1 left the class
  paths at field 1.14x / chain 1.30x / method 1.24x / poly 1.23x / operator
  1.88x / instantiate 2.02x of wren, and called the remainder
  allocation-shaped, assigned to 0.9.0's heavy passes. delta blue lands at
  1.9x — dead center of that prediction. it is instantiate + operator +
  poly-dispatch + short-call churn, exactly the allocation-shaped profile.
  no new surprise, and the first macro-benchmark receipt for the 0.9.0 lane.
- **we beat python3 on wall-clock but not by the usual margin** (2.3-3.9x on
  the micro suite; here 0.81x of python's time, i.e. python is ~1.24x faster
  than oklang). two reasons, both real:
  1. this python version is the tuned one from wren's repo, and 3.12's
     interpreter is fast at exactly this shape (dense method calls on
     slotted classes).
  2. the port pays an oklang-shape tax wren doesn't: **fields are
     ring-private**, so every cross-class read that wren does as a direct
     property load (`v.value`, `c.output`) is a pub accessor method call in
     oklang (`v.value()`, `c.output()`). delta blue is accessor-bound: the
     hot loops touch 2-6 accessors per constraint operation. that is the
     idiomatic oklang shape today — recorded as a finding, not apologized
     for: if accessor-bound macro benches matter, the levers are (a) a
     field-read operator on frozen shapes, or (b) pub fields as a future
     ruling. both are 0.9.0+-shaped conversations.
- **threaded dispatch pays ~2.4%** here (243 vs 250) — consistent with the
  micro suite's switch-vs-threaded deltas.

## files

- `benchmarks/delta_blue.ok` — the port (see its header for the mechanical
  mapping notes: then/otherwise ternary, pub fields for wren's properties
  since the P6 rewrite, pop/push, exclusive-range loops, the no-floor
  note).
- `benchmarks/delta_blue.wren`, `benchmarks/delta_blue.py` — borrowed
  verbatim from wren's repo (upstream license headers retained).
- `delta_blue_bench.py` — the runner (env-overridable paths, cross-check
  built in).

## follow-ups (not done here)

- wire delta_blue into `run-benchmarks.sh`'s hyperfine harness on the
  owner's machine (needs hyperfine; the sandbox used the dedicated runner).
- re-run after each 0.9.0 heavy pass (constant folding, DCE-lite, jump
  threading, tailcalls) — this bench is the acceptance meter for the
  allocation-shaped gap.
- a possible `class_field_loop`-style companion with fields accessed
  ring-internally (no accessors) to isolate how much of the 1.9x is the
  accessor tax vs the allocation tax.

## re-baseline (2026-11-05, session 48): the pub-fields rewrite

the finding above became the fix: patch 51 had already opened the ring
(`pub let`), and P6's first move rewrote the port to use it — every
cross-class accessor call (`v.mark()`, `out.set_walk_strength(...)`,
`this.strength()`) became the direct field access wren's original uses
(`v.mark_`, `out.walk_strength_ = ...`, `strength_`). the rewrite is a
mechanical transformation of the committed file (statement orders,
overrides, and the load-bearing `init` orders all preserved — the
ScaleConstraint constructor assigns its fields BEFORE super, because the
construction chain dispatches into overrides that read them); the diff is
237 lines, every one an accessor-to-field shape change. output unchanged:
1.40654e+07, byte for byte.

the rewrite also found a real compiler bug on its first run:
`obj.field = <call-shaped rhs>` miscompiled — the member store's staged
receiver sat inside the call rhs's callee register window and was
null-filled before the store read it (the subscript lane had the
discipline; the member lane did not). patch 62 fixes it with the same
fresh-rhs-temp rule, plus the regression fixture and the fuzz seed.

| engine | median (before) | median (after) | delta |
|---|---:|---:|---|
| ok (release, -O2) | 249.7 ms | 176.1 ms | -29.5% |
| ok (release, threaded dispatch) | 243.3 ms | 173.4 ms | -28.7% |
| wren_cli | 129.5 ms | 130.4 ms | reference |
| python3 | 201.9 ms | 204.2 ms | — |

relative to wren: **ok 1.35x slower (was 1.93x), ok-threaded 1.33x (was
1.88x)**. the accessor tax was ~30% of the whole benchmark. patch 62's
OP_TAILCALL lane landed in the same window and delta blue did not move
(the benchmark has no tail-shaped returns) — the numbers above are the
new standing baseline for the remaining P6 passes (fusion pass 2,
allocation shaving, the GC question).

honest notes: (1) the remaining 1.35x is now dominated by what PERF-PASS-1
already named — instantiate + operator + poly-dispatch + short-call churn
(the allocation-shaped profile); the shaving pass has its work cut out.
(2) sandbox absolute values wobble ~3% run to run; ratios were stable
across sessions. (3) `Strength.value_` reads inside Strength's own
statics were already ring-legal; the pub is for uniformity with the
Variable surface, not necessity.
