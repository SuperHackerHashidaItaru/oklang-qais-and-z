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

D7 sketch stands: errors as values, bytecode handler frames, **NO setjmp**
(wasm-safe), pcall-shaped native surface now + try/catch sugar later, lazy
tracebacks. lands BEFORE classes because class methods, stdlib API shape,
and every native's error path (dorg3's three missing-return bugs were this
class) hang off the philosophy. the coro⟷error interplay is SETTLED ON
PAPER here (propagation across coroutine boundaries), even though coros
ship later. touches ok_call — the surgical phase.

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

### 0.6.0 — COROUTINES (Q's favorite after classes)

old ladder lean stands: stackful, lua-like — per-coro register file + call
stack, resumer can be the C game loop (proven safe by the array_foreach
callback work). open design round: yield-as-statement vs yield-as-call (Q's
question), and the wasm story (stackful switching on emscripten needs
asyncify or equivalent — decide with measurements, it shapes the embedding
contract). the 0.4.0 error paper decides propagation.

### 0.7.0 — MODULES (language modules)

D8/D9 stand: pipeline isolation + compile-time imports (`import x`,
`import x as z`, `from x import y`), run-once top-level, module registry
keyed by interned path (a 0.2.0-table customer), exports as slot arrays.
the 13.2 lesson rides explicitly: every import adds a compile→load→run
handoff window — the rooting discipline from patches 10/12 is the vaccine,
and the aggressive-gc lane is the proof. **native modules via shared libs
(dlopen) are POST-1.0** — they require a frozen ABI, which is definitionally
a 1.0+ artifact.

### 0.8.0 — STRINGS/UTF-8 + STDLIB GROWTH

oct24 design stands: codepoint API, cached codepoint count, Go-style
U+FFFD lossy decode, ascii identifiers v1. string builder lands here —
explicit, Q's lean (optimization-shaped: chunk buffer + join, hidden from
nobody). the SEMANTICS RULING (len/index = bytes vs runes, R8) is made
EARLY — during the 0.2/0.3 era, as a D-number — because everything
downstream (stdlib signatures, docs, benchmarks, map keys) assumes it;
implementation waits for this phase. std's big push: it grew through every
phase, now it completes ON the module system (std splits into modules as
modules' first customer).

### 0.9.0 — HARDENING + TOOLING

- parser: fuzz harness (clean-parse-or-clean-error, never crash — the
  12.95 flag), error recovery (multiple errors per compile, sync to
  statement boundary), message quality pass. arena/AST decisions if classes
  or utf8 didn't already force them.
- tooling: the CLI stops being "literal ass" — `ok run/test/dump/version`
  candidates, runner UX (failure buckets, timeouts), REPL itch-decided.
- cheap compiler passes: constant folding + propagation on statically
  resolved operands, compare+jump fusion (kills boolean materialization in
  condition position — this IS Q's "branch no materialize"; hard, optional,
  measured before kept), DCE-lite, jump threading. tailcall implementation
  (semantics ruled early — R7).
- bench expansion; everything measured against the workflow baselines.

### 1.0.0 — THE FREEZE

nothing new ships here. the checklist (§6) gets walked, docs get written,
API gets audited, release gets cut. then the post-1.0 era opens with the
web framework as the first big consumer.

## 3. standing lanes (ride any minor)

**PERF** — measured, gated, zero contract change:
- computed gotos behind the OK_PURE-style compile-time gate (Q's main
  course; hidden behind a feature macro exactly because it's non-standard —
  it doesn't break binaries like vectorization might). measure on
  branch_toggle, our worst-case bench.
- dispatch tag-switch micro-opt (Q's perform_as observation: tag-check then
  switch vs the if-chains in the other perform_XX). may be free (compiler
  sees it) or real (it can't know our tag semantics) — measure, don't
  assume. hot-path candidate.
- string interning: separate from hashing-on-construct (which lands with
  maps). interning changes key equality to pointer-eq — a post-classes
  option, not a dependency.
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

**release naming** (Q's question, answered):
- who names releases: distros/OSes — Ubuntu (adjective+animal, alphabetical:
  Jammy, Noble), Debian (Toy Story characters, in order — Perens worked at
  Pixar), NixOS (animals), macOS (California places). languages: almost
  none name releases — they name **features, proposals, eras**: Python 3000,
  the walrus operator, structural pattern matching; C++0x; JS "Harmony";
  Java's Project Loom/Panama/Valhalla umbrella codenames; Rust's editions
  are years, not names.
- why the split: a language version is a compatibility contract keyed by
  tooling; a distro version is a marketing event. names carry zero
  machine-readable meaning, and 0.1.1 doesn't deserve a name.
- verdict: **codename the PHASES now** (commit prefixes + doc titles —
  they're referenced forever, so they stick; pre-1.0 they carry zero legacy
  risk because there's no external surface). **release names post-1.0,
  only when consumers exist to remember them** (Q's buried-name instinct is
  correct — a name needs rememberers). if/when we do: a fixed dictionary
  drawn in order, the Debian/Ubuntu rule shape, so names never repeat or
  collide. the theme is Q's department, obviously — dorg/puer/porgs/
  slanderina/ballerina is a track record.
- until picked, this doc says "the maps phase", "the classes phase".

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
- R4 MR destructuring syntax: `let a, b = f()` vs `let (a, b) = f()`
- R5 errors-as-values surface shape; try/catch sugar timing
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
