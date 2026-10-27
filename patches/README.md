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
