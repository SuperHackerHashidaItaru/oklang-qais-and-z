# spec: numeric for (`for let i = 0 : n? {`) — IMPLEMENTED round 11

date: 26/10 (round 10, draft). 27/10 (round 11, implemented + all four open
questions ruled). status: **IMPLEMENTED** on branch numeric-for-draft
(commits 1501d5a + b4adbe7 + 4c76455), 185/185 e2e, ASan + aggressive GC clean.
supersedes docs/specs/numeric-for.md (oct24).

## rulings (owner, round 11)

1. step syntax: **colon** (`for i = a : b : c`). `by` rejected — common
   variable name, not worth hijacking. "colon is fine, and readable (get used
   to it! lol)".
2. frozen limit: **yes**, lua-style. "this is the main selling point". the
   C-for keeps parsing until the iteration protocol round because it covers
   what the numeric for can't (arbitrary conditions, mutable trip).
3. direction strategy: owner delegated ("even if we take 2 op slots... no need
   to reask"). **decision: zero extra slots** — one fused FOR_LOOP deriving
   direction from the step's sign per iteration. analysis: serial UP/DOWN
   back edges cost the same sign test for positive loops and strictly more
   for negative; selecting one back edge needs body duplication or
   self-modifying bytecode (re-entrancy hazard under recursion with differing
   directions). lua 5.4's float FORLOOP is the same shape. the 2 slots stay
   banked.
4. int48 fast path: **never in v1**. f64 everywhere; the FOR ops accept
   int-tagged values by widening (door stays honest, no fast path).

consistency fix from the owner's review: the draft's optional `let` was wrong —
`for let (mut)? i = ...` now matches for-in and the C-for. no good reason
existed, and optional let created a shadow-vs-assign ambiguity.

## surface (as shipped)

```ok
for let i = 0 : 5? { ... }            // 0 1 2 3 4 — exclusive upper bound
for let mut i = 0 : n? { ... }        // mut allowed: reseats the current
                                       // iteration's value; the trip follows
for let i = 5 : 0 : -1? { ... }       // 5 4 3 2 1 — exclusive lower bound
```

- bounds evaluated once, left to right, at entry. limit frozen. step defaults
  to 1; step == 0 is a runtime error ("numeric for step is zero.").
- direction = sign(step). mismatched direction (start 0, limit 5, step -1) =
  zero trips, no special error. NaN anywhere = empty loop (all comparisons
  false) — documented, not special-cased.
- like sequential lets: the start sees the outer binding (counter
  uninitialized during its resolution), the limit and step see the counter
  (`for let i = 2 : i * 3?` works).
- counter capture: shared slot (closures read current value while running,
  final value after). fresh-binding upgrade parked to the iteration-protocol
  round.

## encoding (as shipped — differs from the draft's sketch)

both ops are 2-word, sharing OP_JUMP_IF's shape so `patch_jump_if` patches
them unchanged:

```
OP_FOR_PREP    2w: (A, B) -> A = forward offset to the loop exit; B = counter
                        register; limit at B + 1, step at B + 2 (compiler
                        allocates adjacently and asserts). validates numbers,
                        errors on zero step, runs the first bounds test, jumps
                        A when out of range.
OP_FOR_LOOP    2w: (A, B) -> A = backward offset to the body top. i += step,
                        sign test picks < or >, jumps when in range.
```

the draft's `p=int_mode` operand is gone (int48 never in v1); the draft's
"(b) literal-step fast path" is obsolete — the sign test inside the single
LOOP op made it unnecessary (see ruling 3).

shape note: PREP does the first test directly rather than lua's
pre-subtract-the-step trick, because start - step can overflow to ±inf and
infinite-loop (e.g. start -1e308, step 1e308); the direct test is exact at
the f64 extremes.

## for-in convergence (the owner's logged arrays-round request)

shipped in the same commit: for-in's hidden locals reordered
(counter/limit/step adjacent, then arr, then the user binding — declaration
order is not evaluation order), the LESS/JUMP_IF/ADD/LOOP tail replaced by
FOR_PREP + FOR_LOOP. per iteration: ARRAY_GET + FOR_LOOP (2 dispatches, was 4).
footnote: a user-reassigned `len` global now fails with "numeric for limit
must be a number" instead of the LESS error — same failure class.

## tests

21 files under tests/e2e/tests/loops/numeric/ (+errors/): counts (0/1/n-trip),
negative step, step from expression, fractional step, frozen limit, bounds
evaluated once (side-effect counter), break/continue, nested with for-in,
nested shadowing, mut reseat, limit-sees-counter, capture, 2^53 trip-count
exactness, step-zero + non-number + immutable-counter + mut-counter-non-number
errors.

## benchmarks (sandbox, 5 runs, indicative)

loop_counter: C-for 586ms -> numeric 246ms (2.4x). arithmetic_sum: C-for
711ms -> numeric 376ms (1.9x). the assign peephole (round 11, separate commit)
took local_pressure 283 -> 230ms. owner's hyperfine numbers are the real ones.
