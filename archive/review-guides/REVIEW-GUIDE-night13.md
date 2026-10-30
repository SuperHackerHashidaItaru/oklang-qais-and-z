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

---

# Addendum — the ~3:30am round (patches 07 + 08, bench read)

You applied 05+06 (`442ad31`, with the `-lm` as `target_link_libraries(ok PUBLIC m)`
— nice, PUBLIC is the right scope). Two more patches are stacked on that, plus
the bench read you asked for (the other LLM's webui froze; mine didn't).

## The bench read (new runs, `93cadc7`)

**The verdict on 06: real but modest. Keep it (it's already in master).**

| bench | old → new (ok, ms) | ratio old → new |
|---|---|---|
| numeric_for_sum | 157.4 → 148.0 (−6.0%) | 2.09× → 1.93× |
| numeric_for_counter | 157.4 → 147.9 (−6.0%) | 2.07× → 1.97× |
| numeric_for_step | 81.7 → 77.3 (−5.4%) | 2.05× → 1.93× |
| array_for_in_sum | 301.4 → 295.3 (−2.0%) | for-in rides the same back edge, diluted by ARRAY_GET |
| array_for_in_sum_indexed | 297.9 → 287.5 (−3.5%) | 〃 |

**Why I trust the direction but not the decimals:** every while-loop bench
(bytecode untouched by 06) also moved — loop_counter flat (−0.4%), assign_add
−5.2%, arithmetic_sum −5.6%, local_pressure −9.1%, global_access −5.6%,
branch_toggle −2.1%, closure/push flat. Unchanged code scattering 0 to −9%
between sessions = a ±5% cross-session noise/confound floor in this run
(wider error bars than last time, lua drifting +1–3% too). The for-family's
−5~6% sits at that band's edge BUT: it moves as a family (3 numeric + 2
for-in), the direction matches my sandbox, and it matches the removed work
exactly. Signal, sized ~5%.

**Two bench-protocol notes for the next optimization round:**

1. the run consolidated to a single `ok-reg` and deleted `ok-reg-old` — the
   two-binary same-session A/B was the right design and is worth restoring
   for optimization landings (cross-session comparisons fight the ±5% floor).
2. `ok-reg` changed size noticeably between runs (254408 → 242072 bytes) —
   if the build flags changed between sessions, absolute cross-session
   deltas carry that confound. pinning `CMAKE_BUILD_TYPE=Release` in
   `run-benchmarks.sh` (and recording it in the results) kills the class.

**Where the remaining ~1.9× lives** (unchanged analysis): the serial chain —
2-word fetch, shared-switch dispatch — plus Lua's FORLOOP being a single
1-word op. That's the post-jam menu: computed-goto dispatch (~60–80 LOC),
1-word FOR_LOOP when B ≤ 255, PGO flags. Not tonight.

## `07-floor-modulo` — your ruling, implemented

Floored, python/lua aligned: `-7 % 3 == 2`, `7 % -3 == -2`, and the payoff
line — `x % n` for positive `n` always lands in `[0, n)`, negative indices
included (`tiles[-1 % 3]` is the last element; pinned in the test).
Implementation: `floor_mod` helper (fmod + one conditional addition of the
divisor), the int48 arm applies the identical adjustment — one rule, both
domains. C's truncated remainder stays host-side (`fmod`) for whoever needs
it, std-lib surface later.

**One future-seam note, now written down:** when integer/floor division
lands, keep it paired with this so `a == floor(a / b) * b + a % b` holds —
python's invariant, and the reason floor-mod + real-division is coherent
today but floor-mod + trunc-int-division tomorrow would not be.

Tests updated to the floored expectations; 203/203 × plain/release/aggressive/
asan/asan-aggr.

## `08-github-ci` — CI on the mirror, status back to codeberg

`.github/workflows/ci.yml`: plain + asan matrix (cmake Release + tests,
ctest, e2e runner, embedding test), then one aggregated job that POSTs a
commit status to `codeberg.org/api/v1/repos/qais/oklang/statuses/{sha}` —
same sha, mirrors share them. Context `ci/github-actions`, target_url = the
GH run's logs, so a click from a codeberg commit lands on the failure output.

**One-time setup, your side:**

1. the GH repo must be a push mirror of codeberg (gitea: settings → mirrors
   → add push mirror; or manual `git push --mirror` from a dual-remote clone).
   If it already is — nothing to do.
2. codeberg token with repository write scope → GH secret `CODEBERG_TOKEN`
   in the mirror repo. Without it, CI still runs and is visible on GH; only
   the cross-post is skipped (by design, the run stays green).

**Details that were load-bearing:** `submodules: recursive` on checkout
(Unity is a submodule — without it the tests build dies), `concurrency`
cancels superseded runs per branch, statuses post on push events only
(PR shas may not exist on the CB side), `fail-fast: false` so both legs
always run.

**Caveat, honestly:** my sandbox has no cmake — the yaml validates and every
path/target name was checked against the tree (`build/ok`,
`build/tests/e2e/oktest-e2e-runner`, `build/tests/integration/embedding/oktest_embedding`,
README-identical invocations), but the first live run on GH is the real test.
Expect possible one-line tweaks (e.g. if the e2e runner exit convention
differs from CI's shell expectations).

## Also in this round

- README template + my draft: `docs/drafts/README-TEMPLATE.md` (what/when
  per section, anti-patterns) and `docs/drafts/README-DRAFT.md` (raw material
  in a neutral voice — including a deliberately-kept broken example with its
  verified fix, which is the template's own rule demonstrated on myself).
- v0.0.5 tag: still not on the remote. recommendation: apply 07+08, then tag
  — ternary, strings, modulo, elision, imm fastpath all make the release.
