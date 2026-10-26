# docs — the waves

oklang collaboration docs come in waves, one directory per wave, newest
last. a wave is written once, then either promoted (folded into the
permanent set) or superseded (marked so). nothing is silently edited.

## permanent/ (oct25 wave — standing documents)

- `PROJECT_STATE.md` — where the project stood after the rewrite: milestone,
  benchmark table + both analyses, known debt, engine/website direction.
- `ROADMAP.md` — pre-halloween final scope + the post-halloween ladder
  (v0.0.5 → v1.0 with rationale).
- `DECISIONS.md` — the decision log, D1–D16, with LOCKED / PROPOSED / OPEN
  status vocabulary.
- `REWRITE_POSTMORTEM.md` — the full register-rewrite arc: 5 phases, 7
  lessons, artifact index.

these are amended only in visible waves (the oct26 delta is the current
amendment pending Q's veto/accept).

## specs/ (oct24 wave — implementation-grade specs)

`arrays.md` (superseded by drafts/ARRAYS-V2.md), `numeric-for.md`
(fused FORPREP/FORLOOP, int/float split, ~2 dispatches/iter target),
`maps-v2.md` (open addressing, linear probe, splitmix64 + cached FNV-1a,
backward-shift delete, lf 0.7), `pipeline-isolation.md` (staging compile +
commit + source release + okc path), `debugger-v0.md` (grounded in existing
line tables/stack traces, OP_BRK patching, run_until stepping), and
`HALLOWEEN-ROADMAP-oct24.md` (the timeboxed crunch plan).

## drafts/ (oct26 wave — **awaiting Q's veto/accept**)

- `DECISIONS-DELTA.md` — rulings/answers/corrections from the owner's
  review round: D1 oracle amendment, D3 rounding answer (not real ≤ 2^53;
  bitwise = JS i32 when it lands), D5 confirmed, D14 resolved
  freeze-then-drop, the _STRICT doctrine (gates strict / chains lenient,
  plus the live ok.c:230 bug + one-line fix), the += accounting (sugar-for-
  names true, general claim retracted), roadmap insertion: arrays →
  defaults → numeric-for → freeze.
- `ARRAYS-V2.md` — the implementation spec for arrays on master: 8 opcodes
  with exact encodings, lvalue subscripts, compound-assign desugaring,
  GC checklist, tests, implementation order. supersedes specs/arrays.md.
- `DEFAULT-ARGS.md` — call-time evaluation design, OP_JUMP_IF_ARGC guard +
  ladder, fresh-mutable-default rationale; kwargs verdict: later,
  compile-time-resolved-only if ever, not python.

**processing rule:** Q reads these, vetoes/accepts per section in chat;
accepted content gets folded into permanent/ in the next docs wave.
nothing here is law until that happens.
