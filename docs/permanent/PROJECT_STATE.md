# oklang — project state (2026-10-25)

what oklang is: a small, zero-dependency C implementation of a language + bytecode
VM. general-purpose scripting language that happens to be good as an engine
language — designed FOR the 2D application engine qais is building toward, but
never shaped AROUND it (explicitly not a gdscript). personal language first:
qais is user #1. current version: **v0.0.4, shipping now**.

## the milestone (today)

**register VM rewrite: DONE** — by the owner's definition: "running the old game
that worked on the old stack vm". dorg3 (~1.5k loc, 86 natives, closures,
native->VM callbacks, music/textures/input/state) runs the full loop: menu ->
waves -> fork spawns -> collisions -> death screen -> retry -> restart.

owner's note on the definition: it is fragile as definitions go ("tests green"
was achieved in round one while the game still crashed four different ways),
but it is the best bar available — and it caught every bug the harness missed.

verification matrix (owner's machine):
- native: full game, fully playable.
- web via emscripten (same no-wasm setup as the old game): runs perfect.
  fps fluctuates around 60 (old build: stable 58) — known issue, not blocking,
  see below.
- debug + OK_AGGRESSIVE_GC + ASan: clean. "super snappy", 1000+ fps on an old
  machine. spikes are gc pauses only (stop-the-world) — expected at this stage.

## benchmarks (2026-10-25, owner's machine, 10 runs each, hyperfine + memory capture)

raw data: upload/combined.json, tables: upload/combined.md, sources:
upload/combined_output.txt (8 benchmarks: arithmetic_sum, branch_toggle,
closure_upvalue, function_calls, global_access, local_pressure, loop_counter,
recursion_fib).

| bench | ok-stack | ok-register | lua | python3 | reg vs stack | reg vs lua |
|---|---:|---:|---:|---:|---:|---:|
| arithmetic_sum | 545.8 | 269.4 | 190.1 | 875.6 | 2.03x faster | 1.42x slower |
| branch_toggle | 464.0 | 266.1 | 132.6 | 626.3 | 1.74x | 2.01x slower |
| closure_upvalue | 49.7 | 35.4 | 31.3 | 98.7 | 1.40x | 1.13x slower |
| function_calls | 83.8 | 59.0 | 57.0 | 144.5 | 1.42x | 1.03x slower |
| global_access | 278.1 | 147.9 | 224.2 | 579.7 | 1.88x | **1.52x faster** |
| local_pressure | 182.1 | 91.5 | 60.9 | 352.7 | 1.99x | 1.50x slower |
| loop_counter | 420.2 | 215.9 | 144.4 | 514.2 | 1.95x | 1.50x slower |
| recursion_fib | 21.6 | 18.6 | 15.8 | 52.5 | 1.16x | 1.18x slower |

memory: all three VMs 3.1-3.5 MB. python3 ~12 MB. the embedding story is
already where it needs to be.

### owner's analysis (recorded — this is the "why", don't lose it)

- **globals win (1.52x over lua)** because globals are index-based, resolved at
  compile time: the compiler emits special load/store ops into an array-based
  global store, skipping name lookup entirely. this is the only supported
  behavior today (unknown name = hard compile error). a name-based path with
  inline caching comes later as an escape hatch — index always beats name, even
  cached/interned. (no string interning exists, by the way — deliberate.)
- **branch_toggle gap**: lua never materializes conditions — compare ops in
  condition position carry a jump hint (the "k" operand IS the jump), and/or
  compile to TEST-style ops + jumps; booleans are only born when stored.
  we always materialize the comparison result, then test it.
- **loops**: unfair — lua's numeric for is one fused FORLOOP instruction; ours
  is a general-purpose C-style for. numeric-for is planned (spec exists).
- **locals**: unknown. owner's guess: compiler materializes unnecessarily.
- **compiler**: two-pass (AST -> compile + codegen) vs lua's single pass. a
  hair slower to compile, but codegen can fold peepholes for free. plenty of
  headroom, deliberately postponed.

### Z's additions

- **the dynamic core is already at parity**: function_calls 1.03x, closures
  1.13x, fib 1.18x of lua. calling convention, upvalue access, frame plumbing
  — the parts that are hard to fix later — are sound. every remaining gap is
  codegen SHAPE, which is cheap to fix and scheduled (v0.0.6).
- **local_pressure hypothesis**: constants. if every `a = a + k` emits
  `LOADK tmp; ADD a, a, tmp` (2 dispatches) where lua emits one `ADD` with the
  constant folded into an operand (RK-style), that is exactly ~1.5x on this
  bench. the VM already HAS constant-operand flags (p/q + resolve_operand) —
  if the compiler simply stops pre-materializing literal operands, this may be
  nearly free. **confirm with the disassembler before touching anything.**
- arithmetic_sum = same constant story + while-loop dispatch overhead.
- stddevs are tight (mostly under 3%): the numbers are trustworthy. the two
  outliers (lua calls max 87ms, ours 76.3ms) look like scheduler noise;
  medians agree with means.
- vs python3: 2.3x-3.9x faster on everything. worth one line in the release
  notes next to the memory number.

## known issues / accepted debt (deliberate, tracked)

- **web fps wobble**: 60+ fluctuating vs old build's stable 58. suspects: loop
  not pinned to requestAnimationFrame, gc running inside the hot frame, the
  emscripten pump mode. fix path: the event-loop lib (post-halloween) gives
  idle/check hooks -> gc in idle, render on rAF. not game-blocking.
- **gc pauses**: stop-the-world mark-sweep. fine for now; incremental gc is a
  post-classes lever. cheap knobs (gc.pause/resume/collect/count) are a
  pre-halloween stretch if the game hitches.
- **exit-time leaks** (~182KB, compiler contexts): cosmetic, known.
- **mixed-type `==` semantics**: open decision, error-model round.
- **parser fragility**: workable but fragile; hardening round scheduled
  (arenas, error recovery, multi-error reporting).
- **TAG_INT exists but the compiler never emits int-tagged values**: harmless
  (numeric literals are raw doubles, comparisons consistent); see DECISIONS D3.

## project direction (owner's stated intent — recorded so it survives)

- origin: game engine developer who hated using lua for scripting; engines
  dormant 2-3 years; oklang is the road back to building.
- target: a multiplatform **2D application engine** — games AND apps, a real ui
  system, real-time (OpenGL) rendering, clean web compilation. oklang is its
  scripting language but stands alone as a general-purpose language.
- the pledge: no portfolio website until the site can be operated entirely
  from oklang (backend + frontend). Z's recorded recommendation: don't gate the
  site on an in-engine HTML renderer — static-site generation in oklang first,
  a dynamic backend once sockets + event loop exist, and the HTML/CSS renderer
  later as an engine UI-system feature, not as the site's foundation.
- the engine speaks canvas via emscripten today (proven by this round's web
  build).
- far-future ambitions parked here so they aren't forgotten: tree-sitter
  grammar exists (github.com/QaisAlsaid/tree-sitter-oklang); LSP and a tiny
  in-house event loop are wanted despite the lua-style minimalism doctrine —
  see DECISIONS D12/D15.

## where everything lives

- repo: codeberg.org/qais/oklang, branch `register`.
- this docs wave: download/docs-oct25/ (this file, ROADMAP, DECISIONS,
  REWRITE_POSTMORTEM).
- previous wave: download/docs-oct24/ (specs: arrays, numeric-for, maps-v2,
  pipeline-isolation, debugger-v0 + the halloween roadmap).
- deep-dive of the gc/asan round: download/PORT_README.md.
- game soak harness: download/dorg3-integration-harness/ (repro/repro2/repro3).
- session-by-session log: /home/z/my-project/worklog.md.

## round 11 addendum (2026-10-27, pre-freeze)

- **numeric-for implemented** (pending Q's review): fused FOR_PREP/FOR_LOOP,
  frozen lua-style limit, colon step, `let` required for family consistency.
  for-in converged on the same back edge. sandbox: loop_counter 2.4x vs the
  C-for, arithmetic_sum 1.9x.
- **disassembler exists now** (was v0.0.6): `ok --dump file.ok`. per-op decode,
  jump targets, constants inline, proto recursion.
- **local_pressure theory REFUTED**: constants already fold into operands; the
  real cost was assign's temp+MOVE pairs. fixed same session (simple-left
  binop rhs -> local's register): 283 -> 230ms sandbox. the two unsafe shapes
  (`a = (a+1)+a`, `a = f()+a`) keep the old path, pinned by regressions.
- v0.0.5 feature set now: arrays, defaults, numeric-for, disassembler,
  assign peephole. remaining before tag: Q's review of the three patches,
  lua-vs-oklang benchmark re-run on the owner's machine, dorg3 migration of
  its ~15 C-style fors (optional for the tag, planned for the 27th).
