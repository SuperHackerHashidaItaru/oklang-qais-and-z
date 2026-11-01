# oklang-qais-and-z

the collaboration layer between qais (Q) and Z for the oklang project.

this repo exists because Z's context resets between rounds and the sandbox
sometimes resets with it. it is the durable memory: round logs, decisions,
specs, drafts, and the recovery protocol for when local state goes boom.

**this repo contains no oklang source code.** the language lives at
`https://codeberg.org/qais/oklang` (master, tagged **v0.1.0** = the
embedding API: roots, traced natives, arrays, print hook). the first game
lives at `github.com/QaisAlsaid/dorg3`. this repo is only the
process/documentation/drafting layer that makes the collaboration survive
round boundaries.


## map

- `Z-GUIDE.md` — the first thing Z reads in any round. who we are, how we
  work, the hard-won rules, the round protocol.
- `WORKLOG.md` — append-only mirror of Z's local worklog. one entry per
  session, `Task ID: session-N`. never rewritten; corrections go in new
  entries.
- `recovery/RECOVERY-PROTOCOL.md` — what to do when Z's local data is
  gone or Z is acting strange. contains the exact `resync` dance.
- **`docs/drafts/ROADMAP-v1.md` — the 0.1.0 → 1.0.0 plan (DRAFT, awaiting
  Q's accept).** the compass doc: phases, standing lanes, versioning +
  naming policy, the 1.0 checklist, open rulings register.
- `docs/drafts/MAPS.md` — the 0.2.0 first-feature draft (read with the
  roadmap). `docs/drafts/WEBSITE-PLATFORM.md` — the parked post-1.0 web
  platform conversation. `docs/drafts/DECISIONS-DELTA.md` — oct26 rulings
  still awaiting their fold into DECISIONS.md (next docs wave).
- `docs/permanent/` — the standing documents: DECISIONS (D1–D16 + delta
  pending), ROADMAP (oct25 ladder — superseded by ROADMAP-v1 on accept),
  PROJECT_STATE, REWRITE_POSTMORTEM, POSTMORTEM-aggressive-gc.
- `docs/specs/` — implementation-grade specs that still feed future work:
  maps-v2 (core algorithm, appendix to MAPS.md), pipeline-isolation
  (0.7.0 modules), debugger-v0.
- `patches/` — **staging area only**: deliverables awaiting Q's review.
  empty right now (everything through 16 is applied). applied patches and
  their review guides live in `archive/patches/` + `archive/review-guides/`.
- `bench/` — the benchmark suite, results, combine tooling. feeds the
  benches workflow when it comes back online.
- `tools/dorg3-harness/` — the no-raylib dorg3 integration harness.
  retired as *authority* per D1 amendment, kept as regression pin.
- `dorg3/` — the shipped-game review (session 13.0).

## repo status notes

- `.github/workflows/benches.yml.disabled` — the bench workflow is parked:
  this repo is private and GH gives private repos no free runners. Q plans
  an account move; when that happens (Q supplies the token), rename back
  to `benches.yml` and do the first manual dispatch — it closes the
  v0.0.5-era results staleness and re-baselines outputs after the `%g`
  change.
- the oklang repo itself stays **minimal** — no website, no site tooling,
  ever. the future web framework gets its own repo post-1.0 (Q's ruling,
  session 17).

## ground rules

1. **append-only.** round history is never rewritten. the worklog gains
   entries; docs get amended in visible waves, not silently.
2. **D-numbers are forever.** decisions are D1–D16 and counting; never
   renumbered, only amended (see DECISIONS-DELTA for the amendment pattern).
3. **drafts are not law.** a draft becomes real when Q accepts it in chat;
   the next docs wave folds it into the permanent set.
4. **no PATs in pushed files.** credentials live in Q's hands and Z's local
   git config only.
5. **the source of truth for code is codeberg.** this repo never becomes a
   code mirror; if it did, it would drift and lie.
