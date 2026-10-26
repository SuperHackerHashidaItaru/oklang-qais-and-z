# spec: numeric for (`for i = 0 : n? {`) — draft for owner review

date: 26/10 (round 10). status: DRAFT — not accepted, not law.
supersedes/augments docs/specs/numeric-for.md (oct24). written against the
arrays commit (`63be373`) — the for-in desugar, hidden-local pattern and
loop-context plumbing it references are all real, shipped code now.

## 1. surface

```ok
for i = 0 : 5? {
  print i;      // 0 1 2 3 4
}

for let mut i = 0 : n? { ... }      // mut binding allowed, same as for-in
for i = 5 : 0 : -1? { ... }         // optional third slot: step
for let mut i = 0 : len(a)? {       // composes with arrays directly
  print a[i];
}
```

- `for (let mut)? IDENT = <expr> : <expr> (: <expr>)? ? <statement>`
- start, limit, step: any expressions, evaluated ONCE, left to right, at loop
  entry. **the limit is frozen**: mutating whatever it read from inside the
  body does not extend or shrink the trip (lua semantics, deliberate departure
  from the C-for's per-iteration condition — the C-for's day job was counting,
  and the numeric for takes that job over completely).
- step defaults to 1. step must be a number; **step == 0 is a runtime error**
  ("numeric for step is zero." — loud, per doctrine).
- direction is derived from the step's sign, not a flag: positive step iterates
  while `i < limit`, negative while `i > limit`. a mismatch of signs
  (start 0, limit 5, step -1) simply runs zero iterations — the condition is
  false on entry. no special error; it composes (`for i = 5 : 5? {}` is also
  empty and correct).
- `i` is a normal binding in the loop's scope: same shared-slot semantics as
  for-in and the C-for today (assignable if `mut`, captureable, per-iteration
  read). a fresh-binding upgrade is a separate, semantic-affecting decision —
  parked to the iteration-protocol round (see §6).
- no float/int split in v1: everything is f64 (D3). exactness holds for
  |values| < 2^53 with whole steps; ULP drift beyond that is documented
  behavior, not a bug to fix.

## 2. the colon family

`:` now means "drawn from" across both loop forms:

```
for let v : arr? { }        // values, drawn from a sequence
for i = 0 : n? { }          // numbers, drawn from a range
```

collision audit (same one the for-in round ran, re-run for the third slot):
- ternary: `then/otherwise` keywords — no `?:`. free.
- map literals `{k: v}`: brace context, parser knows it is not in a for
  header. free (and when maps land, `:` gets a second family meaning —
  key-value; both are "pairing" readings, c++ does exactly this).
- slices `a[m:n]`: bracket context. free if ever wanted.
- labels/goto: not planned.
- the lexer already lexes `:` (TOKEN_COLON existed with a null pratt slot
  before either loop used it).

the step slot `for i = a : b : c` is two colons in one header. the header
parser reads positions, so there is no ambiguity — but if it reads badly to
you, the alternative is `for i = a : b by c` (a keyword, forever) — your call.
the draft's position: keep punctuation, the header shape is positional and
machine-checkable, and keywords are load-bearing forever.

## 3. bytecode

two opcodes, appended to the enum (61 → 63 of 128):

```
OP_FOR_PREP    2w: (p, A, B)  -> validate step != 0 (via r[B]); set direction;
                                 r[A] = r[A] (start stays); no jump in v1
OP_FOR_LOOP    w:  (p, A, B)  -> r[A] = r[A] + r[B] (step);
                                 if (p == 1) == (r[A] < limit) jump back
```

honest accounting of the oct24 "fused" claim, now against real dispatch costs:
- the oct24 target was ~2 dispatches/iter. what the pair above actually gives:
  1 (FOR_LOOP) per iteration, plus the compare inside it — because the loop
  head IS the instruction. the test is `i + step <op> limit` with `op` chosen
  by the p flag at PREP-time... except p is per-instruction, not per-frame.
  so the direction bit must live somewhere readable: the clean options are
  (a) two opcodes (FOR_LOOP_UP / FOR_LOOP_DOWN, +1 enum slot each side), or
  (b) p is fixed at emit time by the compiler when the step is a **literal**
  (covers `step = 1` and `-1`, the 99% case), and the dynamic-step path falls
  back to a compare+jump tail emitted after FOR_LOOP (the desugar shape,
  3 dispatches). recommendation: (b), complexity budget is a real currency
  and literal steps are the game case.
- limit lives in a hidden local (see §4); r[B] is the step register. reading
  operands from registers keeps the decoder uniform — no new operand class.

why not lua's computed trip count (`iterations = (limit - start) / step`)?
f64 rounding on huge ranges makes the count wrong at the edges; test-per-iter
is exact. the oct24 spec assumed int paths made trip counting safe; with
int48 internal and the f64 surface, per-iteration comparison is the honest
shape. revisit if/when the int fast path lands (§7).

## 4. compiler shape — share the for-in skeleton

the for-in desugar established the pattern this round; numeric-for is a
variation on it, and the shared parts should be factored, not copied:

```
begin_scope
  hidden local step? (or register-only)  // " step", untypeable
  hidden local limit                     // " len"-style mangled name
  i = <start>                            // named local, user-visible
  limit = <limit expr>
  step = <step expr or constant 1>
  OP_FOR_PREP (validate, direction)
loop_start:
  compare i against limit (or nothing — fused in FOR_LOOP)
  JUMP_IF exit
  <body>
  continue_target = increment site
  OP_FOR_LOOP (i, step) — increments and jumps back
exit:
```

- same loop_context discipline (continue_forward = true, breaks patched,
  cleanup_scope at scope depth).
- the hidden locals are LOCALS not temps, for the same reason as for-in:
  body statements reset temps to local_top and would clobber them.
- extraction target: `compile_for_in_statement` and the numeric form share
  `begin loop scope + hidden locals + continue/break plumbing` — a small
  helper each for "declare hidden local" and "finish loop context". if the
  sharing feels forced during implementation, copy is acceptable; the spec
  should not mortgage the code to a maybe.

## 5. patch-the-array note (the owner's logged request)

when numeric-for lands: **for-in over arrays should switch its counter
machinery to FOR_LOOP** — same hidden `arr`/`len`/`i` locals, but the
`LESS + JUMP_IF + ADD + LOOP` tail (5 dispatches/iter today) collapses into
`FOR_LOOP` (1). the desugar keeps the `len` call at entry either way. this is
the logged follow-up from the arrays round: the two loop forms converge on
one increment/test instruction.

## 6. C-for retirement (D14, unchanged but now dated)

freeze now, drop after: the C-style for dies when the iteration protocol
lands post-classes. the numeric for plus for-in cover 100% of its live uses
(the game's loops are all `i = 0; i < n; i = i + 1`). the retirement PR
should delete compile_for_statement's C path, the parser's C-for clauses,
and move the C-for tests to their numeric equivalents.

## 7. open questions for the owner (blocking)

1. step syntax: `for i = a : b : c` (two colons) — acceptable, or `by`?
2. frozen limit semantics — confirmed wanted? (lua-style; the C-for re-reads
   its condition every iteration, the numeric for will not.)
3. direction strategy (b): literal-step fast path with p at emit time,
   dynamic step falls back to compare+jump. or spend the enum slots on
   FOR_LOOP_UP/FOR_LOOP_DOWN and always fuse?
4. int48 fast path (oct24's "int/float split"): still wanted as a v2, or is
   f64-everywhere fine until the disassembler/benchmark round says otherwise?

## 8. tests (when it lands)

- basic counts (0-trip, 1-trip, n-trip), negative step, step from an
  expression, huge ranges (2^53 boundary, documented ULP behavior), break/
  continue, nested with for-in, mutation of the limit source inside the body
  (frozen — pin it), step==0 error, non-number start/limit/step errors,
  `mut` binding, capture of i.
- bench: 10M-iteration loop vs the C-for baseline (376ms at port time) and
  vs lua. target: < 150ms (the fused loop should beat the port's 2x-gap
  estimate; if it doesn't, the disassembler tells us why before anything else).
