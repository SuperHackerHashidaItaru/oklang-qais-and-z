# BENCH NOTES — v0.0.5, the clean baseline

read by Z, session 12.95 (night of 27/10). this is the "i wanna be informed post halloween" artifact.

## provenance

- run: `bench/results/` @ collab `4d08852` (2026-10-27 22:21 +0300)
- binary: `bench/ok-reg` (nix glibc 2.42, x86-64) — spot-checked in my sandbox via the system
  loader: `-7 % 3 -> 2`, `7 % -3 -> -2`. **floor ruling is in the benched binary.**
- build: v0.0.5 = `c28d92d` (tagged 21:55, 26 minutes before the run), release flags,
  aggr-gc off, asan off
- protocol: hyperfine `--shell=none`, 5 warmups + 15 runs, lua + python3 as controls,
  frozen `ok-stack` (`267aa53`) on the legacy 8
- previous run for deltas: `93cadc7` (the `442ad31` trunc-modulo build, 2w + 10r)

## what i did beyond reading

1. **validated all 30 benchmark outputs** by running the exact benched binary on every
   `benchmarks/*.ok` and comparing against the documented expected values: **30/30 correct
   numerically**, every hyperfine exit code 0. (21 of 30 render as `%g` scientific notation —
   `5e+13` is `49999995000000`; the values are right, the text is just `%g`. exactly what
   bench/README.md's "validate numeric results, not exact text" note warns about.) the runner
   itself only checks exit codes — this closes that gap for this run.
2. startup floor measured: empty script, min **0.4 ms** — negligible against every benchmark.
3. full re-read of the JSONs: cross-run deltas with lua/python as machine controls,
   compound-vs-plain drift, within-run structural ratios, variance, memory.

## the table

mean ms over 15 runs. cv = stddev/mean. ns/it from min. `—` = not run (stack VM is legacy-only).

| benchmark | ok-reg | cv% | lua | python3 | ok/lua | ok/py | stk/ok | ns/it |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| arithmetic_sum | 264.3 | 0.7 | 184.2 | 2311.8 | 1.44 | 0.11 | 1.97 | 26.2 |
| array_for_in_sum | 283.7 | 0.6 | 486.9 | 1325.7 | **0.58** | 0.21 | — | 35.1 |
| array_for_in_sum_indexed | 285.7 | 2.2 | 210.5 | 1306.0 | 1.36 | 0.22 | — | 35.1 |
| array_index_sum | 370.4 | 2.5 | 272.0 | 1994.3 | 1.36 | 0.19 | — | 45.5 |
| array_literal_alloc | 53.2 | 6.6 | 47.7 | 156.1 | 1.11 | 0.34 | — | 248.8 |
| array_push_growth | 18.2 | 6.7 | 30.6 | 76.1 | **0.60** | 0.24 | — | 83.2 |
| array_subscript_add_compound | 93.7 | 1.0 | 85.6 | 378.7 | 1.09 | 0.25 | — | 46.2 |
| array_subscript_add_plain | 95.5 | 2.3 | 86.8 | 407.1 | 1.10 | 0.23 | — | 46.6 |
| array_subscript_variable_add_compound | 100.8 | 2.1 | 86.2 | 418.7 | 1.17 | 0.24 | — | 49.5 |
| array_subscript_variable_add_plain | 106.5 | 2.4 | 86.4 | 535.6 | 1.23 | 0.20 | — | 51.9 |
| assign_add_compound | 132.6 | 1.5 | 88.6 | 1068.4 | 1.50 | 0.12 | — | 26.1 |
| assign_add_plain | 134.7 | 3.9 | 88.3 | 1065.7 | 1.53 | 0.13 | — | 26.3 |
| assign_div_compound | 56.8 | 2.5 | 38.7 | 445.6 | 1.47 | 0.13 | — | 27.3 |
| assign_div_plain | 58.5 | 3.4 | 39.4 | 442.4 | 1.49 | 0.13 | — | 27.6 |
| assign_mul_compound | 52.9 | 4.9 | 41.9 | 423.1 | 1.26 | 0.13 | — | 25.1 |
| assign_mul_plain | 52.6 | 2.9 | 41.9 | 421.0 | 1.26 | 0.12 | — | 25.3 |
| assign_sub_compound | 122.9 | 3.2 | 89.5 | 946.9 | 1.37 | 0.13 | — | 24.1 |
| assign_sub_plain | 123.3 | 1.3 | 89.5 | 956.0 | 1.38 | 0.13 | — | 24.2 |
| branch_toggle | 262.0 | 2.3 | 128.4 | 1374.5 | **2.04** | 0.19 | 1.71 | 51.6 |
| closure_upvalue | 36.8 | 3.9 | 31.9 | 156.7 | 1.15 | 0.23 | 1.26 | 70.6 |
| function_calls | 59.3 | 2.3 | 50.2 | 284.8 | 1.18 | 0.21 | 1.36 | 56.9 |
| global_access | 157.7 | 2.1 | 214.0 | 558.3 | **0.74** | 0.28 | 1.68 | 30.6 |
| global_array_subscript_add_compound | 99.9 | 1.6 | 135.5 | 377.3 | **0.74** | 0.26 | — | 49.1 |
| global_array_subscript_add_plain | 106.3 | 3.9 | 135.7 | 403.0 | 0.78 | 0.26 | — | 51.9 |
| local_pressure | 92.6 | 1.6 | 58.5 | 818.6 | 1.58 | 0.11 | 1.90 | 91.1 |
| loop_counter | 204.1 | 1.7 | 139.4 | 1157.3 | 1.46 | 0.18 | 1.95 | 20.0 |
| numeric_for_counter | 149.6 | 4.6 | 75.4 | 1575.3 | 1.99 | 0.09 | — | 14.6 |
| numeric_for_step | 76.6 | 1.1 | 39.8 | 910.6 | 1.93 | 0.08 | — | 14.8 |
| numeric_for_sum | 147.8 | 0.8 | 76.2 | 1766.9 | 1.94 | 0.08 | — | 14.6 |
| recursion_fib | 18.5 | 5.0 | 16.2 | 51.2 | 1.15 | 0.36 | 1.12 | — |

geomeans: **ok/lua 1.24x** over all 30 (spread 0.58–2.04) · **ok/python 5.8x** ·
**stack/register 1.59x** over the legacy 8 (spread 1.12–1.97).

## findings

### 1. no regressions, and the noise floor finally shrank

every ok delta vs the previous run is inside ±5%, most inside ±2%; median |Δ| ≈ 1%. the
controls moved too (lua worst −5.6% on function_calls, python ±3.2%), and net (okΔ − luaΔ)
is ±5% worst-case. floor modulo and the formatter run cost nothing measurable — correct,
since no bench touches `%`. so the reworked protocol (5 warmups + 15 runs, pinned release
flags) moved the cross-session band from the old ±5–9% down to ~±2–3% typical. **this
protocol can now read single-digit effects.** that was the point of the rework; it worked.

### 2. the register rewrite is vindicated with the cleanest data yet

stack-vs-register on the legacy 8, same run, same machine, frozen ok-stack binary:
1.12x (recursion_fib) to 1.97x (arithmetic_sum), geomean **1.59x**, with the loop-heavy
end at 1.90–1.97x. the "pyrrhic rewrite" question is closed — with numbers, on the owner's
machine, clean build.

### 3. the fused numeric-for pays, measured inside a single run

numeric_for_sum (fused for + accumulator) 147.8 ms vs arithmetic_sum (while + accumulator)
264.3 ms → **0.56x, a 1.79x speedup for identical work in the same binary**. counter-only:
0.73x. non-unit step: 0.58x. no cross-session caveat this time — same run, same machine
state. this is the cleanest confirmation of the 05+06 fast-path work to date. (for scale:
lua's own for/while ratio is 0.41x — its while-loop is comparatively weaker; oklang's while
holds up better against its own for than lua's does.)

### 4. for-in is a structural win — and beats lua at lua's own game

oklang for-in = 0.77x of manual indexing (283.7 vs 370.4). lua's ipairs = 1.79x of ITS
manual indexing (486.9 vs 272.0) — lua's generic-for protocol pays per iteration,
oklang's array iterator doesn't. net: oklang for-in runs at **0.58x of lua ipairs**.
against lua's best form (the numeric-index comparator added in v3) oklang is 1.36x —
keeping the honest framing: the iterator protocol is the win, not raw element access.

### 5. compound assign: settled numerically, matches the battle-tested langs

ok drift (compound vs plain): add −1.6%, sub −0.3%, mul +0.6%, div −2.9% — no penalty,
mostly a hair faster. python on the same pairs: +0.3 / −1.0 / +0.5 / +0.7 — same noise
band. lua control: ±1.7%. the subscript pairs are where the semantics show: ok saves
1.9% (literal index), 5.3% (variable index), 6.0% (global container) — single evaluation
of container+index in the compound form buying exactly one subscript resolution. python
saves 7.0% and **21.8%** on the same two paths, because its plain form pays double
subscript machinery — same direction, bigger magnitude, because there is more to save.
conclusion: compound paths pay no tax, and the single-evaluation semantics buy what they
should. the `a = a + 1` vs `a += 1` question is now closed with numbers, not just
disassembly.

### 6. the map vs lua — wins where the ISA does work, loses where dispatch is everything

wins: for-in 0.58x, push growth 0.60x, global access 0.74x, global-container subscript
0.74–0.78x. ties: literal alloc 1.11x, subscript add 1.09–1.10x, closures 1.15x,
recursion fib 1.15x, calls 1.18x. losses: dispatch-dense scalar loops — branch_toggle
2.04x (worst), numeric-for family 1.93–1.99x, local_pressure 1.58x, whiles 1.44–1.46x,
assigns 1.26–1.53x. the shape is coherent: whenever one oklang instruction does a compound
job (ARRAY_GET, fused FOR_PREP/FOR_LOOP, resolved global slots), the 2-word encoding holds
or wins; when the loop body is nothing but dispatch, the serial word2 fetch + shared
switch costs the full ~2x. post-jam menu unchanged — first item is still dispatch
(computed goto / direct threading), and branch_toggle + loop_counter remain the rods.

### 7. vs python: 5.8x geomean (3–12.5x spread)

the sanity anchor holds, and it doubles as a floor for the game: oklang scripting will not
be the python-shaped bottleneck.

### 8. memory: lua-class

~3.2–3.4 MB peak on the loop benches (identical to lua), 12.8 MB on literal-alloc (same
as lua), and push growth peaks at 4.0 MB vs lua's 7.2 MB — the growth factor is tighter
than lua's. nothing to do here.

### variance note

median cv 2.3% across the suite; the worst are the short benchmarks (push growth 6.7% at
18 ms, literal alloc 6.6%) — relative noise scales with brevity, as expected. min-based
reads remain the robust statistic.

## housekeeping finds (small, actionable)

- **README release badge renders red** ("no releases or repo not found"). pushing a tag
  does not create a GitHub Release object. fix: `gh release create v0.0.5 -R QaisAlsaid/oklang
  --title v0.0.5 --generate-notes`, or draft it in the UI — 30 seconds. the coverage badge
  is live and reads 53%.
- **coverage signal read** (owner's skim + my source pass): "array new long" = the
  `is_long` emit arms. every emit site splits narrow/long on register indexes or argc
  crossing a_MAX/b_MAX (~255), and nothing in the suite crosses those thresholds. the
  cheap fix is a *generated* test — a >255-element array literal, a function with >255
  constants/locals — five lines of python to generate, and it flips both the compiler
  arms and the VM's long-opcode handlers. "parser fuzzing paths" = error/sync branches
  the `errors/` dir only partially walks; the standard move is a tiny fuzz harness
  (random bytes + mutated valid programs; assert clean-parse-or-clean-error, never a
  crash). both post-jam; neither blocks the game. and the owner's instinct is right —
  the % number is mostly noise (OOM arms); the uncovered *encoding* arms are the signal.
- **runner idea (optional)**: hyperfine checks exit codes only; this read validated
  outputs with a hand script (`scripts/bench_read_v005.py` on my side). a ~10-line
  addition to run-benchmarks.sh (run each .ok once before hyperfine, compare against an
  expected-output file) would make that permanent. also, if python keeps hurting: it is
  the slowest control by ~10x and its cv is tiny — giving python its own RUNS=5 (or a
  FAST=1 mode that skips it) would cut suite wall time roughly in half without losing
  the drift reference.

## one-number summary

v0.0.5, clean release build, owner's machine: **register VM 1.59x geomean over the stack
VM it replaced (1.97x on loop-heavy code) · 1.24x from lua geomean (0.58x–2.04x spread) ·
5.8x over python · lua-class memory · zero regressions · 30/30 outputs correct.** the
remaining gap is dispatch, and it is a known, menu'd enemy.
