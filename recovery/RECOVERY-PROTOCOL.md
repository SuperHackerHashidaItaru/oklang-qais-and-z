# recovery protocol — when local data goes boom

two audiences: Q (the human) and Z (the assistant, who may be reading this
with a blank slate). the protocol is deliberately boring and exact, so a
confused Z can follow it without judgment calls.

## how Q recognizes the failure

Z is acting strange if, in a new round, he:

- asks about files or docs that clearly exist (in this repo or the sandbox),
- re-asks a question that was answered in a previous round,
- proposes to redo work that is already done and recorded,
- seems unaware of the standing protocol (patches-not-pushes, batch rounds,
  the oracle rotation), or
- says anything about "missing" worklog / scripts / clones that Q knows
  are there.

any of these = local state loss or context loss. the response is not to
explain everything from scratch in chat — it is:

## the `resync` move (Q's side)

1. say the word: **`resync`**
2. paste: the repo URL (`https://github.com/QaisAlsaid/oklang-qais-and-z`)
   and a working PAT (fresh one if the old expired — fine-grained, scoped
   to just this repo, is the better long-term habit).
3. optionally: paste the last round's entry from WORKLOG.md into chat as
   a kickstart.

that's all. everything else is Z's job.

## the `resync` dance (Z's side, exact sequence)

1. clone or pull this repo with the supplied credential.
2. read `Z-GUIDE.md` fully.
3. read the last 2 entries of `WORKLOG.md`.
4. read `docs/drafts/` status (anything awaiting veto/accept?).
5. check the sandbox: `/home/z/my-project/oklang` present? if yes, `git
   status` + branch; if no, clone `-b master` (or the branch the worklog
   tail names) from `https://codeberg.org/qais/oklang`, rebuild with
   `/home/z/my-project/scripts/build_ok.sh`, run
   `/home/z/my-project/scripts/run_e2e.sh` — expect 110/110 (or whatever
   the worklog tail says the current count is).
6. announce to Q in one message: state understood, current branch, current
   drafts status, what you believe the next task is. then continue that
   task. do not ask permission to exist.

## data-loss decision tree

- **sandbox wiped, repo alive** (the common case): resync dance, steps
  1–6. the repo carries everything that matters: decisions, specs, drafts,
  round history. rebuild scripts are described in the worklog header if
  `scripts/` died too (gcc 14.2, no cmake, per-TU `-std=c99 -Wall -Wextra
  -Wpedantic` + link cli).
- **repo unreachable, sandbox alive**: work locally, keep the worklog
  appended as usual, and tell Q — he holds a local clone of this repo and
  can reconcile by hand. worst case the repo is reconstructed from the
  sandbox copies of every doc (they all live under
  `/home/z/my-project/download/docs-*`).
- **both wiped**: Q pastes WORKLOG.md from his local clone into chat. that
  plus this protocol is enough to continue; the docs waves get re-uploaded
  from his side.
- **nothing wiped, Z just seems confused**: still do the dance. reading
  the guide is cheap; guessing is not.

## what Z must never do during recovery

- never invent or "reconstruct" history from vibes — if the record doesn't
  say it, it didn't happen.
- never silently fork a parallel state (a second worklog, a second clone
  with different assumptions) — divergence is the actual disaster this
  protocol prevents.
- never treat Q's chat memory as the primary record. the repo is primary;
  chat is transport.
- never claim a deliverable exists without verifying it against the
  actual file tree (`find . -type f`) first. the round-9 incident — this
  very file being reported as delivered while it had never been written,
  because a failed write (missing parent dir) was never retried after the
  mkdir — is the canonical example. verify, then claim.
