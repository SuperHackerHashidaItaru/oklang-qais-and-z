# patches — pending

deliverables awaiting Q's review + hand-application, newest last. apply in
order (each patch is stacked on the previous):

1. `07-floor-modulo.patch` — vs `442ad31` (master after 05+06). your ruling:
   floored modulo (python/lua), floor_mod helper + identical int48 arm
   adjustment, tests updated (`-7 % 3 == 2`, wraparound dogfood). e2e 203/203
   x plain/release/aggressive/asan/asan-aggr.
2. `08-github-ci.patch` — stacked on 07. `.github/workflows/ci.yml`: plain +
   asan on the GH mirror (free runners), unity via submodules, ctest + e2e +
   embedding, aggregated status cross-post to codeberg's statuses api
   (`ci/github-actions` context). needs the `CODEBERG_TOKEN` secret — setup
   notes in the guide addendum. first live run is the real test (no cmake in
   my sandbox).

3. `09-no-question-unbreak.patch` — vs `0a88601` (no-question branch tip,
   NOT master). unbreaks the half-finished rewrite: swapped string_deinit
   args (build), oppAe e-field mask/offset + bool-truncation (every return
   was null — 65/206 failures alone), hidden-scope loop revived + user
   top-level scope + `p_initialized` wiring (core names resolve, no
   `redefining local` at top level), `TOKEN_AS` → `OPERATOR_BIN_AS` mapping
   (`as` used to abort the compiler), as_bool missing else (`null as bool`
   → true), PREC_AS moved between EXPONENT and PREFIX (rust cast level;
   design.txt §1B requires it above SUM), opcode-name table order fix,
   disassembler `as` global-operand print (the branch's own known issue).
   e2e after: 210/211 — the 1 is the open `glob let len` semantics
   decision (REVIEW-GUIDE-round14.md §4, recommendation: keep
   locals-first + add a warning). plus new `operators/as/` tests (the
   operator had zero coverage).

`REVIEW-GUIDE-night13.md` (+ its addendum) — the whole-night bulk-review
guide: per-patch where-to-look, the 06 proof obligations, the bench read
with its noise-floor analysis, the CI setup steps, open items.

after Q applies + pushes, these move to archive/patches/ and this dir empties.

## history

- rounds 10's arrays-v2 + default-args patches: APPLIED by Q (squash
  `9e9fd41`), archived under archive/patches/.
- rounds 11's numeric-for / disassembler / assign-peephole (01-03): APPLIED
  by Q (squash `55e1293`).
- round 12.5's buffer-night patch (04: ternary, string concat, equality fix,
  token purge, elision, window fix): APPLIED by Q (squash `4633dc9`, yolo).
- night-13's 05-modulo + 06-forloop-imm-fastpath: APPLIED by Q (`442ad31`,
  with `-lm` as `target_link_libraries(ok PUBLIC m)`).
