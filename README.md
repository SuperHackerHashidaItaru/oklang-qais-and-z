# oklang-qais-and-z

the collaboration layer between qais (Q) and Z for the oklang project.

this repo exists because Z's context resets between rounds and the sandbox
sometimes resets with it. it is the durable memory: round logs, decisions,
specs, drafts, and the recovery protocol for when local state goes boom.

**this repo contains no oklang source code.** the language lives at
`https://codeberg.org/qais/oklang` (master at v0.4.0 "Eyrie"; the climb
since — 0.5.0 Citadel through 0.8.0 O Resolution — lives on the stacked
staging lanes, pending the ruled release plan). the first game
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
- **`docs/drafts/GUIDE-0.4.0-to-0.8.0.md` — the whole-climb review map**
  (the four lanes + the two follow-ups, every claim verified at
  973bcd0; the era table is the version ledger).
- **`docs/drafts/ROADMAP-0.9.0-draft.md` — the last pre-v1 wave**
  ("Reconciliation"): round two folded, all six survivors RULED, the
  P1-P7 patch series.
- **`docs/drafts/RELEASE-RUNBOOK.md` — the review & release mechanics**
  for v0.5.0 → v1.0.0: fixups at the tip while reviewing, ONE
  autosquash + --update-refs cascade, tags born after, releases
  oldest-first.
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
  17-52 = the climb (maps era through O Resolution plus the two
  follow-ups); 17-48 applied by Q on the codeberg lanes, 49-52 delivered
  with the guide's receipts. applied patches and their review guides
  live in `archive/patches/` + `archive/review-guides/`.
- `bench/` — the benchmark suite, results, combine tooling. feeds the
  benches workflow when it comes back online.
- `tools/dorg3-harness/` — the no-raylib dorg3 integration harness.
  retired as *authority* per D1 amendment, kept as regression pin.
- `dorg3/` — the shipped-game review (session 13.0).

## repo status notes

- `.github/workflows/benches.yml` — the bench workflow is live again
  (the account move happened): weekly monday run + manual dispatch,
  `oklang_ref` picks the oklang ref to build. master is stale at v0.4.0
  until the release plan lands, so dispatches pin lane refs (the
  automated runs currently sit at oklang 0666651).
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
