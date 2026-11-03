# ROADMAP-v1 — 0.1.0 → 1.0.0 (the compass doc)

status: DRAFT awaiting Q's accept/reorder. upon acceptance it supersedes
`docs/permanent/ROADMAP.md` (the oct25 ladder — read its "ordering rationale"
before vetoing; it argues the same bones) and the next docs wave folds this
into permanent/. sources merged: Q's 17-item vibes list (session 18) + the
oct25 ladder's still-alive items (D6/D7/D10/D12/D16 sketches, not-in-v1 list,
parking lot) + sessions 13–17 learnings (dorg3 dogfood weights, the 13.2
handoff-window bug class, the rooting/embedding API now shipping as v0.1.0).

## 0. what 1.0 IS

a **contract freeze**, not "feature complete", not "fast".

> a stranger can embed oklang in a real product, read the docs, and trust
> that what ships won't break them.

everything before 1.0 exists to make that sentence true. speed, LSP,
debugger polish, incremental GC — all real, all AFTER the freeze, because
none of them are contracts. the calendar is vibes (rough horizon at our
cadence: spring–summer 2027; every 0.x minor is independently shippable, so
sliding costs nothing); **the checklist at the bottom is the clock.**

## 1. the shape

- **phases** — one per minor release, sequenced by dependency. 0.2.0 → 0.9.0,
  then the freeze.
- **standing lanes** — perf and hardening work that rides ANY minor. rules:
  measured, gated (211×3 + embed), zero contract change.
- **dogfood lane** — games at Q's pace. each phase names the game feature it
  unlocks. the oracle rotates; each shipped game is the integration test.

## 2. the phases

### 0.2.0 — MAPS

draft: `docs/drafts/MAPS.md` (supersedes `docs/specs/maps-v2.md`, which
carries the core algorithm and survives as its appendix). one generalized
table core (linear probing, backward-shift delete, no tombstones) serving
internals AND surface. string-hash-on-construct rides here (patch C) — it
also kills the 7-transient-allocs-per-run from the 13.2 postmortem.
dogfood unlock: entity props, config tables, content-as-data for the next
game. exit: property test + e2e + embed ×3, bench trio vs lua/python.

### 0.3.0 — SPANS: multiple returns + varargs

D6 sketch stands: register-window results, argc as a frame field, results at
callee base (zero copy), static result arity with destructuring. the
return_1 fast path survives: caller-declared arity keeps single-result calls
on it — this is the thing lua couldn't cleanly keep, and a register VM with
caller-known arity can. std seed lands here (the functions MR enables).
syntax ruling: destructuring shape (R4).

### 0.4.0 — ERRORS (the user-facing model)

**RULED (E1-E8) + IMPLEMENTED — patches 23-27 awaiting Q's hand-application**
(`patches/`, rulings + honest deltas in `docs/drafts/ERRORS.md` §9/§11).
D7 sketch stands: errors as values, bytecode handler frames, **NO setjmp /
no longjmp** (Q's explicit ruling — don't inherit lua's bad parts; also
wasm-safe), lazy tracebacks. try/catch-vs-pcall is answered AND SHIPPED,
split by boundary: try/catch/finalize for oklang code, status-shaped
ok_call + ok_last_error/ok_native_throw for C (E7: host-caught errors
print nothing). source ownership shipped as OPT-IN retention (E8's catch:
ok_retain_source, default off — borrow for the call, intern the path
forever, chunks carry source ids; the full keyed registry stays 0.7.0
below). the error_at* overhaul, gcc-style located notes, and the from-field
deletion all landed (patches 23/24/25). v1 limitations deliberate (§11):
break/continue/return skip finalize, REPL per-line numbering (0.9.0 REPL
pass owns it — the deleted from-field's future consumer).
lands BEFORE classes because class methods, stdlib API shape, and every
native's error path (dorg3's three missing-return bugs were this class) hang
off the philosophy — the caught-value {:kind:,:what:} map is the exact
shape 0.5.0's std.exception wraps. the coro⟷error interplay is SETTLED ON
PAPER here (handler records live in frames, coros own their stacks,
unwinding is coro-local by construction), even though coros ship later.

### 0.5.0 — CLASSES + THE TYPE SYSTEM (the 1.0 centerpiece)

D10 sketch stands: frozen shape, copy-down single inheritance, metaclass
statics, primitives share one class object per type, monomorphic inline
caches, compiler-enforced definite init, best-effort dtors, explicit
close/defer for determinism. PLUS the type-system rework Q's list adds:
proper native types replacing the tag hack, `as` growing into real type
values, the conversions system (`operator MyClass` = user-defined
conversion + operator overloading), classes exposed through the native API,
hierarchy + introspection. dorg3's #1 pain (porgs = 4 parallel arrays,
every texture = 3 globals) dies here. may split 0.5.0 core / 0.5.x polish —
it's the biggest phase. note: method tables are a 0.2.0-table customer —
another reason maps precede classes.

### 0.6.0 — COROUTINES (Vertigo) — Q's favorite after classes

old ladder lean stands: stackful, lua-like — per-coro register file + call
stack, resumer can be the C game loop (proven safe by the array_foreach
callback work). shape is basically LOCKED per Q; what remains is plumbing
+ design details at implementation time. wasm: NOT a blocker — the common
game use case is single-function coroutines, and -sASYNCIFY exists for the
rest; native is the primary consumer, wasm is one consumer. open design
rounds: yield-as-statement vs yield-as-call (Q's question), error
propagation (partially solved — the 0.4.0 paper finishes it).

### 0.7.0 — MODULES (language modules)

D8/D9 stand: pipeline isolation + compile-time imports (`import x`,
`import x as z`, `from x import y`), run-once top-level, module registry
keyed by interned path (a 0.2.0-table customer), exports as slot arrays.
**the source registry lands here (carried from the 0.4.0 errors session):**
replaces last-source-wins with many simultaneously-live sources — the ok
state owns a source set keyed by interned path, chunks reference source
ids, runtime reports resolve id→source for excerpts. 0.4.0's ownership v1
(own-the-copy + chunk identity) is the bridge that makes the swap
mechanical. EXPORT SEMANTICS OPEN (Q's first instinct: implicit exports —
flagged, no ruling yet). the 13.2 lesson rides explicitly: every import adds a
compile→load→run handoff window — the rooting discipline from patches
10/12 is the vaccine, and the aggressive-gc lane is the proof. **native
modules via shared libs (dlopen) are POST-1.0** (Q agreed after the
ABI argument) — they require a frozen ABI, which is definitionally a 1.0+
artifact. **sandboxing lands in the same era** (see 0.9.0) — module loading
and capability exposure are one design surface for an embedded language.

### 0.8.0 — STRINGS/UTF-8 + STDLIB GROWTH

oct24 design stands: codepoint API, cached codepoint count, Go-style
U+FFFD lossy decode, ascii identifiers v1. **two additions from Q's
session-19 review: string ESCAPING (the vital one — `\` handling proper)
and string INTERPOLATION (potential — design when the phase opens).**
string builder lands here — explicit, Q's lean (optimization-shaped:
chunk buffer + join, hidden from nobody). the SEMANTICS RULING
(len/index = bytes vs runes, R8) is made EARLY — during the 0.2/0.3 era,
as a D-number — because everything downstream (stdlib signatures, docs,
benchmarks, map keys) assumes it; implementation waits for this phase.

**stdlib shape (the Q question, answered in layers):**
- **std-zero** — what exists today: the hidden-scope builtins (len, push,
  pop, remove, print, and now get). tiny, forced, fine — it is the
  language's prelude, not a library (every lua does this).
- **oklang-written std** — pure-language modules (string helpers, sort,
  etc.): shipped as .ok source, compiled at load exactly like user
  modules. zero ABI exposure — it's just source.
- **native-backed std** — things needing C (math beyond basics, io, time):
  registered through the SAME module registry but STATICALLY LINKED, not
  dlopen'd. the core↔std ABI is internal and versions together in one
  repo/tarball — the stability contract only matters at the dlopen
  boundary, which is post-1.0. this is exactly how python ships `math`.
  no forced hidden state (they're importable modules), no exposed ABI,
  nothing forced on users (import what you want; std-zero stays tiny).
- **scheduler: NOT std.** coros ship primitives; the event loop is a
  host/embedder concern (D12's event-loop lib is post-1.0). the language
  never forces a policy. games bring their own loop.
- scope verdict: lua-shaped-small core, grown by itch. "the stdlib is
  complete when adding more feels like padding."
std's big push completes ON the module system (std splits into modules as
modules' first customer).

### 0.9.0 — HARDENING + TOOLING

- parser: fuzz harness (clean-parse-or-clean-error, never crash — the
  12.95 flag), error recovery (multiple errors per compile, sync to
  statement boundary — the parser's sync_state leak gets formalized
  here), message-quality residue (author-token choices, pluralization
  sweep — the format itself, true columns, excerpts and notes landed in
  0.4.0; the session-19 "columns are file offsets" observation was
  root-caused in ERRORS.md §3-R1 and fixed by patch 23). arena/AST
  decisions if classes or utf8 didn't already force them.
- **sandboxing** (Q's session-19 addition, pairs with 0.7.0 modules):
  capability model for what an embedded oklang VM may touch — which
  natives/modules a host exposes, load/IO gating, fuel limits (already
  flagged for the /play era). an embedded language ships a contract about
  what guest code CANNOT do.
- tooling: the CLI stops being "literal ass" — `ok run/test/dump/version`
  candidates, runner UX (failure buckets, timeouts), REPL itch-decided.
  docs tool: **okdocy** (Q's name, adopted).
- cheap compiler passes: constant folding + propagation on statically
  resolved operands, compare+jump fusion (the general boolean
  materialization kill — its condition-lane half moved up to the perf lane's
  pass 1 per the session-30 split; hard, optional, measured before kept),
  DCE-lite, jump threading. tailcall implementation (semantics ruled
  early — R7).
- bench expansion; everything measured against the workflow baselines.

### 1.0.0 — THE FREEZE (Celestial)

nothing new ships here. the checklist (§6) gets walked, docs get written
(okdocy's debut), API gets audited, release gets cut. then the post-1.0
era opens with the web framework as the first big consumer.

## 3. standing lanes (ride any minor)

**PERF** — measured, gated, zero contract change. **Ruled (Q, session 30): the
optimization work splits into two passes.** a lightweight pass lands right
after the classes theme closes; the heavy compiler passes stay later.

*pass 1 — the lightweight round: **LANDED (patch 36, session 31).** the
report card is docs/drafts/PERF-PASS-1.md; the split below kept as the
record of what was ruled.*
- string interning: separate from hashing-on-construct (which landed with
  maps). interning changes key equality to pointer-eq — now scheduled here.
  **landed NAMES-only: literals stay uninterned because === is word
  identity (R19 open — interning literals would flip `"a" === "a"`).**
- inline polymorphic cache: the per-site mic grows a 2-3 entry class probe
  chain, so `class_poly_dispatch` (two classes alternating at one site)
  stops thrashing. the class-resident operator cache (patch 32) already
  covers the operator lanes. **landed: 3-way chain, round-robin hand.**
- conditional materialization: conditions stop materializing booleans into
  registers — the compare+jump fusion's first half (this is Q's "branch no
  materialize", sized down to the condition lane; the full fusion stays in
  pass 2). targets the branch_toggle gap. **landed: OP_JUMP_IF_CMP, all
  eight comparators, operators still dispatch through the fused lane.**
- threaded dispatch loop behind a compile-time switch: computed gotos
  (labels-as-values), the classic register-VM dispatch win. **landed:
  CASE/DISPATCH macros, one handler body for both modes, gates run on
  BOTH modes (the switch suite passing is not evidence for the threaded
  build — that exact asymmetry hid the transform bug, see the
  PERF-PASS-1 receipts).**
- pure mode + compiler auto-detection in cmake: the OK_PURE-style build mode
  and cmake detecting GNU extensions (gcc/clang) to enable them — the switch
  the threaded dispatch rides on, and the portability gate that keeps the
  non-GNU builds honest. **landed: OK_PURE option + GNU/Clang auto-detect.**
- sequencing rule: the classes benchmarks (incl. the wren column) run BEFORE
  this pass lands and again after — the before/after numbers are the pass's
  report card. **ran: baseline (classes-staging, -O2) vs after-switch vs
  after-threaded — the core flipped ahead of wren (1.17-1.29x on the loop
  family), the class paths narrowed to 1.14-2.02x, the remainder is
  allocation-shaped (0.9.0's lane).**

*pass 2 — the heavy compiler passes (0.9.0 as planned, standing lane):*
- constant folding + propagation on statically resolved operands.
- full compare+jump fusion (the general boolean-materialization kill, beyond
  the condition lane), DCE-lite, jump threading.
- tailcall implementation (semantics ruled early — R7).
- already landed, for the record: the dispatch tag-switch micro-opt (the
  performs rewrite, patch 31) and the class-resident operator caches
  (patch 32).
- **wasm32 alias-window spike** (survivor of the site postponement — now a
  language-level portability item since wasm is an embedding target):
  re-run the nan-boxing alias tests under emscripten. dorg3 running fine is
  one heap layout, not proof; strict macros are the net, the test proves
  the net. cheap, early, non-blocking.

**HARDENING** — fuzz, more tests, leak/warning cleanup, coverage takes
(the 12.95 items: generated >255-operand tests, parser fuzz corpus).

**DOGFOOD** — games at Q's pace. game-with-sister = current features;
the next one gets maps + spans; the one after that is CLASSES (porgs as
objects, the original itch).

## 4. deliberately NOT pre-1.0 (and where they live)

| item | where | why |
|---|---|---|
| native modules (dlopen) | post-1.0, ~1.1 | needs frozen ABI — that IS the 1.0 artifact |
| web framework + portfolio site | post-1.0, own repo | Q's session-17 ruling: "full minimal web framework, reusable, then sites" — it's the 1.0 PROOF, not a prerequisite |
| incremental / generational GC | post-1.0 perf epoch | non-contract; keep write-barrier headroom during 0.5.0's object-model work so we don't paint ourselves out |
| NaN-boxing rework (kill int branch, compact bits) | post-1.0 perf | semi-contract via wasm windows; spike early, implement late |
| vectorization | post-1.0 exploration | honest: dispatch won't vectorize; array/map kernels might, behind macros. portability value > speed value for an embedded language |
| serialization + okc exe | post-1.0, with the framework | rides pipeline isolation's compile result; bytecode-at-load was a site question, source-at-load won v1 |
| debugger v0 | itch-driven, spec exists (`docs/specs/debugger-v0.md`) | line tables + OP_BRK + run_until stepping — the run_until machinery already exists |
| LSP phases | post-1.0 | phase 1 (tree-sitter wrapper) is independent and can start any time; semantic phases want the stable compiler |
| JIT, threads, int scalars, regex, package registry | NOT in v1 (standing) | unchanged from the oct25 ruling; int scalars revisit only as typed array buffers |

## 5. versioning + naming policy

**semver, pre-1.0 discipline** (Q's instinct, formalized):
- `0.MINOR.PATCH`. **any added capability = MINOR** — feature, surface
  syntax, ok.h API, CLI flag. "if the diff adds a capability, it's a minor."
  no exceptions, however small the feature.
- **PATCH = fixes only.** never a feature, never surface change.
- **breaking = MINOR pre-1.0** (semver's 0.x license), but batched at phase
  boundaries; master between tags is the dev branch and may break freely.
- post-1.0: classic. breaking = MAJOR (rare, epochal), features = MINOR,
  fixes = PATCH. deprecation = two-minor notice once consumers exist.
- the C API (`ok.h`) is the sharpest contract — its changes drive version
  bumps harder than syntax does.
- each minor: tag + `gh release create` + curated CHANGELOG.md entry
  (starts at 0.2.0). backfill: v0.1.0 + v0.0.5 release objects still owed.

**release naming** (RULED by Q, session 19 — codenames are on, for fun,
semver stays canonical): every MINOR gets a codename pre-1.0 (minor is our
major), only MAJORS post-1.0. names needn't always mean something — the
language is personal. current book: **v0.1.0 = Credence**, **coros =
Vertigo** (moonlapse vertigo), **v1.0.0 = Celestial** ("the celestial
touch from grey to black, a fathomless void enclosing. unwritten secrets
beneath the cobwebs. i can not endure" — the amen corner). the theme is
Opeth for now, may continue or not. (history note: distros name releases
— ubuntu's adjective+animal, debian's toy-story characters, nixos animals;
languages name features/proposals/eras — python 3000, the walrus, C++0x,
JS Harmony, java's Loom/Panama/Valhalla. Q's scheme splits the difference:
contract keyed by semver, personality keyed by codename. it works.)

**process (immediate, cheap):** dev branch + protected master on codeberg;
CI once the account-move happens (see §7); benches workflow re-enabled then.

## 6. the 1.0.0 checklist (the clock)

- [ ] surface stable: maps, spans, errors, classes+types+conversions, coros,
      modules, utf8 semantics, stdlib v1 scope cut AND documented (D16)
- [ ] embedding API v1 frozen + audited: roots, tracers, arrays, maps,
      classes, print hook, error handling from C
- [ ] implementation-defined behavior documented: tag layout invariants,
      stack limits, map iteration order, GC timing, number formatting
- [ ] docs: language reference + embedding guide (README grows up, or a
      docs/ dir — NOT a website; that's post-1.0 framework territory)
- [ ] distribution: tagged tarball, one-command build; nix flake; cmake
      install; consider the lua-style amalgamated single .c + .h as an
      embedding convenience
- [ ] test story: e2e + unit + property + fuzz green; aggressive-gc lane
      permanent in CI; bench workflow live
- [ ] process: dev branch + protected master, CHANGELOG, release hygiene
- [ ] the bar: a stranger could embed and ship — the post-1.0 web framework
      PROVES it

## 7. open rulings register (running)

- R1 missing map key: null (lean) vs error — argued in MAPS.md
- R2 -0.0 / 0.0 as map keys: normalize (lean) — MAPS.md
- R3 map iteration order: unspecified v1, documented — MAPS.md
- R4 MR destructuring syntax: `let (a, b) = f()` — CLOSED (spans R1, nov 1)
- R5 errors-as-values surface shape; try/catch sugar timing — drafted
  (ERRORS.md E1–E8), Q ruling pending
- R6 coro: stackful vs stackless; wasm switch story
- R7 tailcall guarantee: yes/no (rule early, implement 0.9.0)
- R8 utf8 len/index semantics: rule early (0.2/0.3 era), implement 0.8.0
- R9 C-style for: KEEP — settled by usage + tree-sitter (closed)
- R10 phase codenames: Q picks as phases open
- carried parking lot: mixed-type `==`; implicit `self` resolution order;
  REPL locals persistence; register-file UINT16_MAX clamp; ordered maps
  (post-1.0, content work will itch)

## 8. what Z added that Q's list didn't have

docs + distribution as 1.0 blockers (the stranger bar is unreadable
without them); the tailcall ruling as an early contract decision; the
wasm32 alias spike surviving the site postponement; CHANGELOG + release
hygiene; the amalgamation option; the deprecation policy. what Z moved
from Q's list: most of #7 (optimizations) and #4 (branch opts) → the perf
lane / post-1.0, because they're not contracts; #5's native modules →
post-1.0 with the ABI argument. everything else kept its place or moved
for dependency reasons argued inline.
