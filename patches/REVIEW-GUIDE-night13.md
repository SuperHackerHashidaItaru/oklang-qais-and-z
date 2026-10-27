# Review guide — night 13 (the night cap)

The whole night in one bulk-review pass. Three layers, two of them not yet applied:

| Layer | What | State |
|---|---|---|
| `04-buffer-night` | ternary, string concat, equality fix, token purge, elision, window fix | **already in master** (`4633dc9`, yolo'd) |
| `05-modulo` | `%` and `%=` | patch against `4633dc9`, commit `ec4e02b` |
| `06-forloop-imm-fastpath` | PREP normalization + immutability-proven raw reads | patch stacked on 05, commit `3789e48` |

**Isolation is exactly what you asked for:** 05 and 06 are separate commits on
`night-13`; `git revert 3789e48` kills the loop experiment without touching
modulo, and vice versa. Nothing else sits between them.

**Both were verified 203/203 e2e × plain/release(-O2)/aggressive/asan/asan-aggr**
in my sandbox, on top of your `4633dc9`.

## 0. Build breakage to expect (read first)

`perform_modulo` calls C99 `fmod`, which lives in **libm**. Your CMakeLists has
no `target_link_libraries` at all, so the link will fail with
`undefined reference to fmod` until you add, next to `add_library(ok ...)`:

```cmake
target_link_libraries(ok m)
```

(My sandbox script needed the same `-lm`.) This is the only build-system
consequence of the night.

## 1. `05-modulo` — the tiny one

| File | What |
|---|---|
| `okchunk.h` | `OP_MODULUS`/`OP_MODULUS_LONG` after DIVIDE (enum shift — no serialized bytecode exists, no consumers, fine) |
| `okvm.c` | `perform_modulo` mirroring `perform_divide`: number/int arms, `modulo by zero.` error, `invalid operands for binary '%'.` |
| `okparser.c` | **the one real fix**: `TOKEN_MODULO_EQUAL`'s pratt row pointed at `parse_binary_infix` — `x %= 3` hit the infix range assert. Now `parse_compound_assign_expression` |
| `okcompiler.c` | `OPERATOR_BIN_MODULO` emission + `OPERATOR_MODULO_EQUAL` in the compound switch |
| `okdebug.c` | names/symbol/width (the generic binop decode arm does the rest) |
| tests | `operators/modulo.ok`, `errors/modulo_zero.ok`, `errors/modulo_invalid.ok` |

**The decision to ratify — truncated, not floored.** `-7 % 3 == -1` here;
Lua and Python floor to `2`. Why I picked trunc: the dormant int48 path is C's
`%` by definition, and `fmod` is its exact f64 twin — one rule across both
domains, no seam. Game usage (parity, wraparound on non-negative indices) is
identical either way. If you want Lua-alignment instead, it's a 3-line change
in `perform_modulo` only (floor-mod formula), and I'd then mirror it in the
int48 arm — say the word.

Fun bug found in my own patch while testing: the error message
`"invalid operands for binary '%'."` goes through `vfprintf` — the bare `%`
ate itself and printed nothing. Now `'%%'`. Worth remembering next time an
operator message contains an operator.

## 2. `06-forloop-imm-fastpath` — the gap experiment

The new benches put the numeric-for family at **2.05–2.09× vs Lua** (sum 157.4
vs 75.5, counter 157.4 vs 76.1, step 81.7 vs 39.9) while everything else sits
at 1.2–1.6×. Reading the codegen showed FOR_LOOP paying three per-trip tag
checks it mostly didn't need. The patch removes them where provable:

- **FOR_PREP normalizes** the counter/limit/step triple in place to
  number-tagged doubles (int-tagged operands widen once at entry).
- **FOR_LOOP p flag** = compiler's immutability proof. `p == 1` (plain `let`,
  or for-in's hidden counter): all three operands read as raw doubles — zero
  tag checks on the back edge. `p == 0` (`let mut`): counter stays gated.
- `emit_incomplete_for_jump` gains the imm param; `patch_jump_if` preserves
  `p` when rewriting `A` (verified in source).
- Disassembler prints `imm` on the back edge; `okchunk.h` documents the
  contract inline.

**The proof obligations — this is where to spend your review time.** The raw
reads are only sound if nothing can store to `r[B]..r[B+2]` between PREP and
each LOOP:

1. immutable counters: enforced at every store path — pinned by
   `loops/numeric/errors/immutable_counter.ok` (compile error on `i = 3`)
2. limit/step: nameless hidden locals (` limit`/` step`, space-mangled) — no
   AST node can name them
3. calls: callee windows open at temps ≥ local_top, locals sit below — the
   session-12.5 window bug was temp-vs-temp ordering, never temp-below-local
4. upvalues: reads don't write the slot; stores to captured mutables are the
   mut path; `close_upvalues` copies out, never writes the register

Existing tests pin all of it: `mut_reseat` (p=0 numeric), `capture` (escaped
closure reading the final counter through the closed upvalue — the subtlest
one), `mut_counter_non_number`, the f64-extreme `two_pow_53_boundary`, plus
the whole for-in family (now always `imm`).

**Honest performance read.** On my box: ~0–5% (counter flat, step −4%,
for-in in the same band). The removed checks were predicted branches that
OoO was already hiding; I verified in `-S` that the imm path compiles to
exactly the intended tag-check-free ~20 instructions — the codegen is right,
the µarch just doesn't care much *here*. Your rig showed FOR_LOOP ops
disproportionately expensive (7.9 vs 5.2 ns/op asymmetry mine doesn't have),
so the hyperfine rerun is the arbiter, as agreed. If it's flat there too:
revert `3789e48`, nothing else moves.

**What's left on the gap, priced** (for the post-jam menu):
computed-goto dispatch (~60–80 LOC, classic 10–25% on dispatch-bound loops,
touches everything) · 1-word FOR_LOOP encoding when B ≤ 255 (kills the serial
word2 fetch, but `patch_jump_if` assumes 2-word layout — encoding surgery) ·
PGO build flags (free, your side) · Lua-style trip-count int path (biggest,
semantics-sensitive — the f64-honesty ruling stands in its way).

## 3. Since 04 went in unreviewed — the 5-minute re-checks

Full guide is `REVIEW-GUIDE-round12.md`; if you only re-check three things on
the yolo'd commit:

1. `assign_call_rhs_window.ok` — the pre-existing bug the elision tests
   flushed out (rhs compiled below the staging; now a fresh temp above it)
2. `strings/equality.ok` — the `strncmp` prefix bug (`"ab" == "abc"` was
   **true** on master before); you'd have hit it in the game's string compares
3. `ok --dump bench/array_subscript_add_plain.ok` — 3 dispatches/iter now
   (was 5), and confirm globals are *unchanged* bytecode (control group)

## 4. Apply + verify recipe

```bash
git apply 05-modulo.patch        # then 06 on top; or cherry-pick ec4e02b 3789e48
# add target_link_libraries(ok m) to CMakeLists
cmake --build ... && ctest       # expect 203/203 + your unit suite
./ok --dump tests/e2e/tests/loops/numeric/basic.ok   # FOR_LOOP ... imm
./ok tests/e2e/tests/operators/modulo.ok             # sign cases, %g output
./ok -e 'print(-7 % 3)'          # -1 — the trunc decision, visible
```

Then the agreed protocol: you rerun the bench suite on your machine, we read
the numeric-for family (and the flat-controls sanity) together next round.

## 5. Open items

- **v0.0.5 tag — still not on the remote** (checked tonight: latest is
  v0.0.4). Master has moved past `55e1293`; my lean: tag the freeze *after*
  applying 05+06 so ternary/strings/modulo are in the release — they're
  user-facing language surface. Your call; the annotated message from session
  12 needs a one-line feature list update either way.
- `value_debug_print` prints INT-tagged values as `null` (latent, D3-gated —
  nothing produces INT tags today; PREP normalization actually re-tags the
  loop triple away). One arm to add whenever int48 wakes up.
- e2e runner still can't pin empty-line expects (round-12 note, unchanged).
