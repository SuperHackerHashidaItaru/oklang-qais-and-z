# round 11 review guide — numeric-for, disassembler, assign peephole

three patches, apply in order, on top of master `9e9fd41`:

```
git apply 01-numeric-for.patch
git apply 02-disassembler.patch
git apply 03-assign-binop-direct.patch
```

state after all three: **185/185 e2e** (182 existing + 21 numeric-for + 2 assign
regressions), clean under ASan and OK_AGGRESSIVE_GC, trace builds
(`-DOK_DEBUG_DUMP_CODE -DOK_TRACE_EXECUTION`) compile and run.

sandbox timings (5 runs, wall clock, shared cpu — indicative, your hyperfine
run is the real number):

| bench | master 9e9fd41 | this branch | |
|---|---:|---:|---|
| local_pressure | 283ms | 230ms | 1.23x |
| loop_counter (while) | 577ms | 521ms | 1.11x |
| loop_counter (C-for) | 644ms | 586ms | 1.10x |
| loop_counter (numeric) | — | 246ms | **2.4x vs C-for** |
| arithmetic_sum (C-for) | 833ms | 711ms | 1.17x |
| arithmetic_sum (numeric) | — | 376ms | **1.9x vs C-for** |

---

## your two questions, answered first

**consistency (`for let mut? i` vs `let mut? i`)** — you were right, there was
no good reason. the draft made `let` optional as a C-family habit; it also
quietly created a shadow-vs-assign ambiguity with an outer variable of the same
name. fixed: `let` is required, same as for-in and the C-for:
`for let (mut)? i = 0 : 5?`. the whole family reads the same now.

**direction strategy** — i took zero extra enum slots, and here is why. the
2-slot option (FOR_LOOP_UP / FOR_LOOP_DOWN emitted serially at the back edge)
does not actually buy anything: with a single compiled body the two back edges
execute serially, so the UP op still has to test the step's sign to know
whether to act — identical per-iteration cost for positive loops, and strictly
worse for negative ones (two dispatches + two tests). the only way dual ops win
is if loop entry could select which back edge runs, which needs either body
duplication or self-modifying bytecode — and self-modifying breaks re-entrancy:
a recursive function whose loop direction differs per frame would resume on a
rewritten back edge. so: one fused FOR_LOOP that derives direction from the
step's sign per iteration (lua 5.4's float-loop shape). the sign test is loop
invariant, the branch predictor eats it, and the 2 slots stay banked.

---

## 01-numeric-for.patch (1037 lines — ~250 are load-bearing)

**what it is**: `for let (mut)? i = <start> : <limit> (: <step>)? ? <stmt>`,
two new opcodes, and for-in converging on the same back edge (your logged
request from the arrays round).

**where to actually read** (in this order):

1. **`src/okchunk.h`, the last two enum entries** — the entire contract is in
   the comments. note the layout: offset rides in A (so the existing
   `patch_jump_if` patches them unchanged), counter register in B, and the
   limit/step live at B+1/B+2 — the compiler declares the three adjacently and
   asserts it. that adjacency is the one real encoding trick in the patch.
2. **`src/okvm.c`, `OP_FOR_PREP` / `OP_FOR_LOOP` cases** (~60 lines with the
   `for_operand_as_number` helper above `resolve_array_base`). PREP validates,
   does the first bounds test, skips empty ranges. LOOP is the whole hot loop:
   add, sign test, compare, jump. limit/step are trusted after PREP (hidden
   locals, unreachable by name) — only the counter is re-gated, because `mut`
   can reseat it.
3. **`src/okcompiler.c`, `compile_for_numeric_statement`** — mirrors
   compile_for_in_statement's skeleton (loop context, dedicated scope, hidden
   locals, continue-to-increment, break patching). one semantic worth knowing:
   the counter stays uninitialized while the start expression resolves
   (so `for let i = i + 2 : ...` sees the outer i, like a let initializer),
   then goes live — the limit and step see it (`: i + 3` uses the counter).
   there is a test pinning both (`limit_sees_counter.ok`).
4. **`src/okcompiler.c`, `compile_for_in_statement` diff** — the convergence.
   hidden locals reordered (counter/limit/step adjacent, then arr, then the
   user binding — declaration order is not evaluation order), the
   LESS/JUMP_IF/ADD/LOOP tail (4 dispatches/iter) is now FOR_PREP + FOR_LOOP.
   per iteration: ARRAY_GET + FOR_LOOP. one behavioral footnote: if someone
   reassigns the `len` global to a non-number-returning function, the error
   now says "numeric for limit must be a number" instead of
   "invalid operands for binary '<'" — same class of failure, different
   message. everything else is byte-for-byte the old semantics.

**noise you can skim**: okast.c/h (the standard four lifecycle functions),
okparser.c (parse_for_numeric_rest is a close copy of parse_for_in_rest — the
interesting 20 lines are in parse_for_statement where the head expression is
parsed once and discriminated on its terminator: `:` → numeric, `;` → C-style),
and the 21 test files.

**semantics pinned by tests, in one list**: exclusive bounds both directions
(up runs while i < limit, down while i > limit — symmetric, so
`for let i = 5 : 0 : -1?` prints 5..1), limit frozen (mutating the source
inside the body does nothing — the selling point), bounds evaluated once
left-to-right, step==0 errors, direction mismatch = zero trips, `mut` reseats
the current iteration's value only (and the trip follows — documented, one
line in mut_reseat.ok), counter capture is shared-slot (closures see the
current value while running, the final value after — same as the C-for today;
the fresh-binding upgrade stays parked to the iteration-protocol round),
2^53-exact trip counts below the boundary. NaN anywhere = empty loop
(all comparisons false) — documented behavior, not special-cased.

## 02-disassembler.patch (714 lines — mostly one new table)

**what it is**: the v0.0.6 placeholder in okdebug.c replaced with a real
per-opcode printer, plus `ok --dump file.ok` (compile + dump, no execution).

**where to read**:

1. **`src/okdebug.c`** — the whole thing is one file. the opcode name table
   (62 entries, compile-time length assert against the enum), then
   `debug_disassemble_instruction`'s big switch — operand decoding per the
   okchunk.h contracts, jump targets resolved to absolute addresses,
   constants/globals inlined, source lines printed on change.
   `debug_disassemble_function` dumps a function then recurses into the
   protos in its constants (luac style). the old dead field-machinery
   (`disassemble`/`disassemble_field`) is deleted.
2. **`src/ok.c` + `include/ok/ok.h`** — `ok_debug_dump_bytecode(ok, enabled,
   skip_run)`, hooked between compile and interpret. ~25 lines total.
3. **`cli/src/main.c`** — `ok --dump <path>`, 6 lines.

**also upgraded for free**: the compile-time `OK_DEBUG_DUMP_CODE` and
`OK_TRACE_EXECUTION` paths now print the new format (verified with a trace
build, not just the plain one).

**noise**: the name table is 62 lines of strings; the switch is repetitive by
nature. `okdump` output example is at the bottom of this file.

## 03-assign-binop-direct.patch (108 lines — all load-bearing)

**what it is**: the local_pressure fix, found by the disassembler the same
hour it landed.

**the finding, precisely**: the oct25 theory ("every `a = a + k` emits
LOADK tmp; ADD a, a, tmp") is **refuted** — constants already fold into
operands (`ADD r11 = r2 + k10 '1'`, zero LOADK). the real cost was that the
assign compiled its rhs into a scratch temp and moved it back:
`ADD r11 = r2 + k10 '1'; MOVE r2 = r11` — 2 dispatches where 1 suffices,
18 per iteration in local_pressure's body.

**the fix**: when the lvalue is a simple local and the rhs is a value binop
whose **left operand is an identifier or literal**, compile the rhs straight
into the variable's register: `ADD r2 = r2 + k10 '1'`. safe because value
binops write their destination exactly once, at the final op, reading both
operands at emit time — the destination may alias an operand. plus a
`materialize_value` self-move guard (register already in dest → no-op).

**the two landmines that keep the old path** (both have regression tests):
`a = (a + 1) + a` and `a = f() + a` — shapes where the variable is re-read
after the left side may have staged intermediate state in the destination.
`assign_operand_read_timing.ok` also pins that operands are read at op time
(a mutating call on the right of `a = a + f()` observes the mutation first).

**where to read**: the gate comment in `compile_assign_expression` (20 lines),
the 3-line guard in `materialize_value`, and the two regression tests. that is
the entire patch.

---

## what i did NOT touch

- the C-for (still parses, still passes — its retirement is still parked at
  the iteration-protocol round per D14)
- compound assign (`a += 1`) — it has its own compile path and the same
  temp+move shape; natural follow-up, not rushed into this patch
- the MOVE staging before ARRAY_GET/CALL arguments (visible in any dump:
  `MOVE r8 = r1; MOVE r9 = r3` before ARRAY_GET) — that is register-window
  calling convention, a bigger design question than a peephole

## suggested verification sequence

```
git apply 01-numeric-for.patch && bash scripts-ish build
./ok tests/e2e/tests/loops/numeric/basic.ok        # 0 1 2 3 4
./ok --dump tests/e2e/tests/arrays/for_in.ok       # see FOR_PREP/FOR_LOOP in for-in
git apply 02-disassembler.patch && rebuild
./ok --dump scripts/bench/local_pressure.ok        # count the ADD+MOVE pairs: zero
git apply 03-assign-binop-direct.patch && rebuild
run the full e2e suite
```

## disassembler output sample (for-in over an array, post-convergence)

```
0006 0000040c          LOAD_GLOBAL      r4 = g0 '<native function len a: 1 0x...>'
0007 00100601          MOVE             r6 = r1
0008 02010415          CALL             r4 = call r4, argc 1, ret 1
0009 00500503          LOAD_CONSTANT    r5 = k5 '1'
0010 0300073c 00000000 FOR_PREP         i=r3 limit=r4 step=r5, empty -> 0019
0012 00100801          MOVE             r8 = r1
0013 00300901          MOVE             r9 = r3
0014 09080737          ARRAY_GET        r7 = r8[r9]
0015 07020620          ADD              r6 = r2 + r7
0016 00600201          MOVE             r2 = r6
0017 0300073d 00000000 FOR_LOOP         i=r3 += r5, -> 0012 (back)
```
