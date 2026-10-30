# patches — the staging area

deliverables awaiting Q's review + hand-application live here, newest last,
stacked in apply order. **when Q applies + pushes, they move to
`archive/patches/` and this dir empties.**

right now this dir is EMPTY — everything through patch 16 has been applied
and pushed (oklang patches 12+13+14 = `83dd40c` merged to master, now
tagged v0.1.0; 16 landed in the tree-sitter repo). the per-patch notes and
contracts live with the archived patches in `archive/patches/` and in
`archive/review-guides/` (REVIEW-GUIDE-night13/round11/12/14/15). the
aggressive-gc postmortem moved to `docs/permanent/`.

next expected occupants: the maps series (draft in
`docs/drafts/MAPS.md` — patch A table core, patch B surface, patch C
hash-on-construct).

## history

- round 10's arrays-v2 + default-args patches: APPLIED by Q (squash
  `9e9fd41`).
- round 11's numeric-for / disassembler / assign-peephole (01-03): APPLIED
  by Q (squash `55e1293`).
- round 12.5's buffer-night patch (04: ternary, string concat, equality
  fix, token purge, elision, window fix): APPLIED by Q (squash `4633dc9`).
- night-13's 05-modulo + 06-forloop-imm-fastpath: APPLIED by Q (`442ad31`).
- 07-floor-modulo + 08-github-ci: APPLIED, tagged v0.0.5.
- 09-no-question-unbreak + 10-gc-guard-stack-and-okrun-rooting +
  11-as-tests: APPLIED by Q (`52b3055`), merged to master `bc272e3`.
- 12-rooting-and-traced-natives + 13-array-public-api + 14-print-hook +
  15-ci-aggressive-lane: APPLIED by Q (`83dd40c`), later follow-ups
  (debug-mode tests, `%g` number formatting, corpus update) → master
  `9ad153c`, tagged **v0.1.0**.
- 16-tree-sitter-oklang-no-question-update: APPLIED by Q to the
  tree-sitter-oklang repo (grammar + queries + corpus, 131/131).
