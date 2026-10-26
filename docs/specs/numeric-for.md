# spec: numeric-for (target: 25/10, first feature after the boot fix)

goal: replace the C-style for as the loop construct, with two fused ops and
zero per-iteration allocation. C-style `for` keeps parsing until after
Halloween (warning: "c-style for is deprecated, will be removed"), hard-drop
in the post-halloween cleanup with a game-code migration pass.

## surface

```ok
for let i in 0..10 ?            // exclusive upper bound: 0..9
  print i;

for let i in 0..=10 ?           // inclusive: 0..10
  print i;

for let i in start..end ?       // bounds are any expressions, evaluated once
  ...

for let i in 0..n step 2 ?      // optional step (v1.1 if the 25th runs out)
```

- `i` is a fresh immutable binding per loop body scope (shadowing rules same
  as any local). no `mut` — you don't assign the counter. wanting to mutate
  it is a smell; use while.
- bounds evaluated exactly once, before the first iteration. `end` changing
  during the loop does nothing (matches rust, unlike C).
- if both bounds (and step) are int-tagged at runtime -> integer loop math
  (exact, no float drift); otherwise double math. this mirrors lua 5.4's
  integer for exactly, including "0..(2^53)" not being an int loop.
- empty range (start >= end for positive step) = zero iterations.

## encoding

three hidden registers at the top of the frame's temp window (allocated by
the compiler like any temps, released at loop end):
  R[i]   the counter
  R[lim] the limit
  R[step] step (v1: implicit 1/-1 folded into the op's p flag; v1.1 real step)

| op | layout | semantics |
|----|--------|-----------|
| OP_FOR_PREP | a'=R[i], b'=offset, p=int_mode | if !(step>0 ? i<lim : i>lim) jump offset; else fall through |
| OP_FOR_LOOP | a'=R[i], b'=offset, p=int_mode | i += step; if still in range jump offset (back to body top); else fall through (exit) |

- offset is relative to the word after the instruction — **same base rule as
  every other jump** (see okchunk.h contract comments; patch_jump already
  encodes this — reuse emit_loop's machinery, do NOT write a third offset
  convention).
- both ops are 1 word narrow, long variant only if a register index demands
  it (offsets can borrow the JUMP width discipline: narrow 12-bit, long 24).
- FOR_LOOP does: load i, add, compare, branch in C without touching the
  constant pool — constants are pre-loaded into R[lim]/R[step] by PREP.
  target shape: ~2 dispatches/iter vs ~7 today.

## int/float mode

p bit selects int math (`VALUE_AS_INT` / `INT_AS_VALUE`) vs double math. the
compiler can't know statically (bounds may be runtime values), so PREP
computes the mode from the actual tags and encodes it... where? PREP's p is
static. instead: PREP reads the tags at runtime and NORMALIZES: if both
bounds are int-tagged it coerces the hidden registers to int mode and p=1
ops stay correct; else everything runs in double. simplest correct v1:
always double math except when both bounds int-tagged, handled by two FOR
op pairs selected by a runtime branch at PREP time (PREP rewrites its own
successor? no — keep it boring: PREP jumps into either the int LOOP or the
double LOOP variant; 4 opcodes total, all trivial).

## migration

```
for let mut i = 0; i < n; i = i + 1?    ->    for let i in 0..n ?
for let mut i = a; i <= b; i = i + s?   ->    for let i in a..=b step s ?
```

dorg3.ok has ~15 C-style fors; migrate on the 27th when the game lands on
the new build.

## tests
0..0 (empty), 0..=0 (one iter), negative direction via step, i immutable
(assign -> expect error), bounds with side-effecting calls (evaluated once —
count prints), int loop past 2^31 (no drift), float loop 0..1 step 0.1 (10
iters, no off-by-one — this is THE test for the int/float split), closure
capturing i (captures per-iteration value — hidden register is COPIED into
the body scope's local, so each closure sees its own i... decide: per-iter
copy (rust-like) vs single var (C-like). recommend per-iter copy; it's what
the closure_loop regression test expects conceptually), 10M bench = target
<200ms.
