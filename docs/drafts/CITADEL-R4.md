# oklang 0.5.0 (Citadel) — classes round 4, the door

status: patch 35, the last of the classes plan, written against your
classes-staging line (30 through 34 applied, the state R9-R16 gated green
on). your four vetoes were four confirmations; this round closes the class
story's code. also in the round: wren joined the bench matrix, the classes
benchmark family landed with it, the optimization pass got its split
recorded in the roadmap, and the test-writing flushed out one real
pre-existing bug in the shipped classes code (§2) — the round's receipt.

## §0 — your rulings, applied

- **R13 confirmed**: `===` is primitive identity, `==` is the user-facing
  overloadable one. the box clarification stands as shipped: `1 === 1.0` is
  true (no public int box), `0.0 === -0.0` is false. your note that `===`
  will be used for classes mainly is the right read — `c1 === c2` on
  instances is word identity, and `is`/`isinstance` cover the class-shaped
  questions.
- **R14 confirmed**: `is` is the exact-class probe — instance-vs-class
  identity, no chain walk. the family question lives in `isinstance`.
  nothing moved; `is_operator.ok` still pins it.
- **R15 deferred as you ruled**: str carries no methods in 0.5.0. the str
  surface grows when a stdlib exists to mirror — recorded in the roadmap's
  0.8.0 lane, where it always lived.
- **R16 confirmed — patch 35 landed**: copy-on-redefine, local classes earn
  per-definition bodies. §1 is the record, including the one shape it
  refuses and why.

## §1 — patch 35: copy-on-redefine

the R2 §5 analysis, implemented with one refinement your ruling forced:
**the copy is gated on the same parent.** the bake encodes the flattened
layout (field slots, method-vs-global lanes), and the flattened layout is a
function of the body AND the parent chain — `fu make(p) { class Inner : p
{...} }` called with two different bases would need two different bakes
from pre-freeze code the proto no longer carries (the freeze rewrites in
place). same parent, both classes frozen against identical sealed chains,
means the inherited bake is valid for the second definition exactly as for
the first — so that case copies and succeeds, and the different-parent case
keeps an honest refusal with its own message ("a local class can only be
redefined with the same parent"). this is the "when applicable" of your
ruling, made precise; the general form (pristine pre-bake code kept around
for rebaking) is the mapped future step if a varying-base factory ever
itches.

the copy itself, `copy_proto_tree` in okvm.c:

- one fresh `object_function` per proto in the tree — code array, line
  table, identifiers (dynamic strings re-owned, static shared), upvalue
  descriptors (they reference the same enclosing chunk, so they copy
  as-is), scalars. constants are shared values except nested non-member
  protos, which copy recursively; member protos of a class declared inside
  the body stay shared (their own OP_CLASS_END owns them — the R2 freeze
  boundary, unchanged).
- mic slots copy as FRESH empties, sized to the original count: each
  definition's sites start cold. sharing would be sound (the key compare
  just misses) but permanently thrashy; fresh is the honest choice. (R18
  below carries it as a veto point.)
- the entry point changed shape: `freeze_patch_closure` now takes the
  closure, not the proto, because the rebind target is `closure->function`
  — the closure's upvalue array was sized from the original proto's
  descriptor count at creation, and the copy's count is identical, so no
  resize can be needed. the nested walk stays proto-level; every proto
  reaching it is fresh or already this class's, so its foreign-marker arm
  became an internal error.

**the rooting discipline** — the part the aggressive-gc lane had to prove.
the copy is a deep tree built under allocation, and allocation can collect
(every allocation, in that lane). the scheme that keeps it sound with a
bounded guard stack: children are created, appended to a safe parent's
constants, THEN filled. a node is safe the moment it lands in a safe
parent's constants, so the guard stack never holds more than the tree root
plus the child being appended — depth two, always, regardless of lambda
nesting. `create_object_function` makes exactly one allocation (its collect
runs before the object exists) and `object_store_append` is an intrusive
prepend that does not allocate, so the window between creation and the
first guard is empty by construction. a failed copy leaves the partial
tree unreachable — the collector reclaims it, nothing dangles.

what flipped:

- `local_class_twice_error.ok` is dead; the R2 `make_class()` example is
  now a positive test (`local_class_twice.ok`) — both generations
  construct, dispatch, and answer `is` distinctly, and one source line
  with alternating receivers proves each generation's method cache is its
  own.
- new pins: `local_class_redefine_inherit.ok` (a named base — the
  inherited-field bake rides the copy), `local_class_redefine_closures.ok`
  (lambdas inside a redefined method capture through the copied
  descriptors), `local_class_redefine_in_method.ok` (the class-in-method
  boundary under re-entry), `local_class_parent_error.ok` (the
  different-base refusal).
- the embed test grew a factory called twice through `ok_call` — the
  C-api lane of the same story, each generation carrying its own captured
  seed.

gates on the final state: **e2e 399 x4** (394 + 6 new - the flipped error
test), unit x3, embed x3. fresh-apply verified on the classes-staging line.

## §2 — the receipt: bare calls lose to the hidden prelude

found while writing the embed factory test, and it is **pre-existing** —
verified on the unpatched session-29 state. a bare name in call position
inside a method resolves through the compiler's ladder as
locals → upvalues → statics → fields → globals, and the std-zero builtins
(len, push, pop, remove, print, get) live in a hidden scope compiled as
enclosing-scope locals — so they are found by the **upvalue** rung, above
the field/method rung. a class with a method named `get` can therefore
never bare-call it: `get()` dispatches the map builtin; `this.get()` is the
only lane that reaches the method. the freeze bake's own ladder
(field → method → global) is correct — the compiler just never emits a
name-op for these names, it captures the builtin directly.

pinned as `bare_builtin_shadow.ok` with both lanes exercised
(`this.get()` → the method, bare `get(m, "b")` → the builtin). this is
R17 below: keep (an honest "the prelude is an enclosing scope" reading,
explicit `this.` escapes the shadow) or fix (the field/method rung moves
above the upvalue rung for hidden-scope bindings — resolve() surgery,
blast radius every class body). i lean keep-and-document for 0.5.0; the
method names that collide are exactly the std-zero five, and `this.` is
already the idiom for the fused fast lane.

## §3 — wren in the matrix, the classes family with it

- **33 `.wren` ports** of the existing suite: every benchmark whose shape
  wren can express. no column for the compound-assign benches (wren has no
  compound syntax — same story as lua), `mr_call` (no multiple returns),
  `varargs_pack` (no varargs). `function_calls` uses a static method —
  wren's free function; `recursion_fib` recurses inside one;
  `map_for_in_sum` iterates `m.keys` (wren's map iteration yields
  `"key:value"` strings, a 0.4 quirk worth knowing). all 33 cross-checked:
  same output as the `.ok` twin under ok/lua/python/wren, values compared
  numerically (oklang's `%g` prints 6 significant digits).
- **six classes benchmarks**, all four languages (lua via metatables,
  python via classes, wren via classes): `class_field_loop`,
  `class_method_call`, `class_dispatch_chain` (method inherited two levels
  up — lua and wren walk the chain every call, our mic caches it),
  `class_operator_add` (Vec2 `+`), `class_instantiate`,
  `class_poly_dispatch` (two classes alternating at one site — the inline
  polymorphic cache's target, sized for the optimization round).
- **the runner and the workflow**: `WREN` env (optional, like OK_STACK,
  per-benchmark on `.wren` presence), a `classes-benches.txt` list plus a
  one-script probe of the register binary — the six class benches are
  skipped with a printed reason until the benched build speaks classes, so
  today's master keeps the auto-run green. the CI builds wren-cli 0.4.x
  itself, best-effort: a failed wren build drops the column, never the
  run.
- **the honest baseline** (sandbox, -O2, best-of-3, indicative — the CI
  matrix is the record): oklang at parity or ahead on the core —
  `loop_counter` dead even, `branch_toggle` slightly ahead,
  `numeric_for_step` ~3x ahead, `map_for_in_sum` ~4.6x ahead. wren ahead
  on the class paths: ~1.2x field loop, ~1.3x method call,
  ~1.35x poly/dispatch chain, ~2x operator add, ~2.1x instantiate. that
  gap is exactly what the lightweight optimization round aims at, which is
  why you ruled it lands with the profiling. "bare minimum we same perf"
  is already true of the non-class core; the class lanes are the work.

## §4 — the optimization split, recorded

ROADMAP-v1's PERF lane now carries the two-pass structure as you ruled it.
**pass 1, the lightweight round, right after classes close**: string
interning; the inline polymorphic cache (the mic's 2-3 entry probe chain);
conditional materialization (the condition-lane half of compare+jump
fusion — "branch no materialize", sized down); the threaded dispatch loop
behind a compile-time switch; pure mode plus cmake's GNU-extension
auto-detection. **pass 2, the heavy compiler passes, 0.9.0 as planned**:
constant folding + propagation, the general fusion, DCE-lite, jump
threading, tailcalls. the sequencing rule is in the roadmap too: classes
benches run before pass 1 lands and again after — before/after is the
report card. two items are recorded as already landed rather than
re-planned: the performs tag-switch rewrite (31) and the class-resident
operator caches (32).

## §5 — what remains of 0.5.0

nothing. the classes plan is code-complete: 30 through 35 written, gated,
delivered. what stands between here and the tag is your side — apply
30-35, push master, tag Citadel when you're satisfied. the flagged
not-blockers that survive into later rounds: tree-sitter catch-up (`is`,
`===`, the classes grammar — external repo, one patch, whenever you want
the corpus current), Exception as the one subclassable native (C16),
kwargs (deferred again), the interpolation hook consulting `operator str`.
then the optimization round: delta blue joins the matrix there, next to
the profiling.

## §6 — open for your veto (R17-R18)

- **R17 the prelude shadowing** (§2): a bare call inside a method reaches
  the std-zero builtin before the class's own method of the same name;
  `this.method()` is the escape. keep and document, or move the field/
  method rung above the hidden-scope upvalue rung. i recommend keep for
  0.5.0.
- **R18 the copy's fresh caches**: each redefinition's mic slots start
  empty (sharing the original's would be sound but permanently thrashy).
  also carried here: the parent-identity gate itself — the
  different-parent factory stays refused, per §1. both are contained
  reverts if you hate them.

## §7 — applying

`git apply patches/35-copy-on-redefine.patch` on top of 30-34, in order.
verify: `build_ok.sh all` + `asan-aggr`, then the full runner — the new
floor is **399 x4 + unit x3 + embed x3**. the new suite:
`classes/local_class_twice`, `local_class_parent_error`,
`local_class_redefine_inherit`, `local_class_redefine_closures`,
`local_class_redefine_in_method`, `classes/bare_builtin_shadow`, and the
embed factory section. formatting hand-matched as always; your
clang-format remains the canonical pass.

— z, session 30. Citadel: the door is hung. what's left is the polish.
