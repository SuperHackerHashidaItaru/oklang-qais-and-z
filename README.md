# oklang-qais-and-z

the collaboration layer between qais (Q) and Z for the oklang project.

this repo exists because Z's context resets between rounds and the sandbox
sometimes resets with it. it is the durable memory: round logs, decisions,
specs, drafts, and the recovery protocol for when local state goes boom.

**this repo contains no oklang source code.** the language lives at
`https://codeberg.org/qais/oklang` (master, tagged v0.0.4 = register VM).
the first game lives at `github.com/QaisAlsaid/dorg3`. this repo is only
the process/documentation/drafting layer that makes the collaboration
survive round boundaries.

## map

- `Z-GUIDE.md` — the first thing Z reads in any round. who we are, how we
  work, the hard-won rules, the round protocol.
- `WORKLOG.md` — append-only mirror of Z's local worklog. one entry per
  session, `Task ID: session-N`. never rewritten; corrections go in new
  entries.
- `recovery/RECOVERY-PROTOCOL.md` — what to do when Z's local data is
  gone or Z is acting strange. contains the exact `resync` dance.
- `docs/permanent/` — the oct25 wave: project state, roadmap, decisions
  (D1–D16), rewrite postmortem. these are the standing documents.
- `docs/specs/` — the oct24 wave: implementation-grade specs (arrays,
  numeric-for, maps-v2, pipeline-isolation, debugger-v0, halloween roadmap).
  superseded where oct26 says so.
- `docs/drafts/` — the oct26 wave: ARRAYS-V2, DEFAULT-ARGS,
  DECISIONS-DELTA. **status: awaiting Q's veto/accept pass.** drafts are
  not law until accepted in chat and folded into the permanent docs.
- `archive/patches/` — the historical patch sets (all applied + pushed to
  codeberg by Q). paper trail only.
- `tools/dorg3-harness/` — the no-raylib dorg3 integration harness
  (repro/repro2/repro3 + stubs). runnable any time the VM changes; retired
  as *authority* per D1 amendment, kept as regression pin.

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
