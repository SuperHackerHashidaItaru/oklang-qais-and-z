# oklang - the review & release runbook: v0.5.0 Citadel -> v1.0.0 Celestial

status: DRAFT round two (session 42). this is the git mechanics for the
ruled release plan (ROADMAP-0.9.0-draft, "the release plan" section):
run the 0.9.0 wave to v1.0-rc, review the whole climb, amend per era,
then release every version v0.5.0 through v1.0.0 as its own release.
round two folds Q's correction: the tidy branch is retired from the
pipeline entirely (it stalled at the bare coros commit, missing the
Epigone patches - everything was done in coros instead), so 0.6.0
pins the coros sha inside coro's own history and 0.7.0 Epigone IS
coro's tip.

Q's own words for the loop this formalizes: "what i was trying to do is
review each version starting from v0.5.0 (citadel) and when i spot
anything worth editing i edit, but i do the edit on the latest version,
then i apply so it cascades for other previous versions, then once i am
done i push to master, and tag and release each one?" - yes. that is
exactly the plan below, with the sharp edges named and the commands
written out. the companion reading: GUIDE-0.4.0-to-0.8.0.md is the map
of WHAT to review, this runbook is HOW the fixes travel.

## section 0 - the situation

the debt: master sits at v0.4.0 (Eyrie, e2e 342x4). everything since,
five versions of work, lives on the stacked staging lanes and has never
been released:

| version | codename | patches | lane / cut point | e2e at landing |
|---|---|---|---|---|
| 0.4.0 | Eyrie | 23-28 | master (released) | 342x4 |
| 0.5.0 | Citadel | 30-37 | classes-staging tip | ~400 |
| 0.6.0 | (none yet) | 38 | 1672c21 "coros" (inside coro's history) | ~410 |
| 0.7.0 | Epigone | 39-42 | coro tip (a8c4a99) | 427 |
| 0.8.0 | O Resolution | 43-52 | modules-staging at 973bcd0 | 510x7 |
| 0.9.0 | Reconciliation | 53+ | the coming wave (P1-P7) | - |
| 1.0.0 | Celestial | - | the rc after the review | - |

(0.6.0 earns a codename if one fits before tag day; nothing breaks if
it never does. the 0.8.0 cut is Q's call during the pin pass: 973bcd0
is the patch-52 head the guide verified; anything that lands on the
lane before the wave opens moves the pin.)

[session 42, Q's correction, RULED: the tidy branch participates in
NOTHING. "it was missing something and i decided to do all in coros
instead" - tidy still points at 1672c21, the bare patch-38 coros
commit, while coro carries the whole Epigone stretch (39-42) on top
of it. consequences, all folded below: rel/v0.6 pins the coros sha
directly (no tidy involved - the sha is coro's own ancestor), rel/v0.7
pins coro's tip, and tidy appears in no command in this runbook. it
can be deleted from codeberg whenever convenient; its sha stays
reachable through rel/v0.6 forever.]

two properties we get to keep, and they make everything else easy:

1. **master never moved.** v0.4.0 is still the merge-base of every
   lane. the whole rewritten history roots exactly at master's tip, so
   the final push to master is a plain fast-forward. no force, no
   surgery on anything public.
2. **nothing from the climb is tagged.** v0.1.0 was the last tag; every
   version since exists only as branch tips. tags are cut AFTER the
   history is final - the one rule that makes rewriting safe is "tags
   never move", and these tags are not born yet.

## section 1 - the plan in one page

five moves:

1. **review per era** - walk the climb oldest-first with three-dot
   diffs (merge-base to tip), the guide as the map, the design drafts
   (CLASSES, COROS, MODULES) as the law.
2. **edit at the tip** - every fix is a commit on the rc head, the
   newest tree, where the fix is written against code that actually
   exists and the full gate matrix can run. never edit inside an old
   era directly.
3. **label the target while it is hot** - reviewing era E and spot a
   fix? commit it as `git commit --fixup=<sha-in-era-E>` right then.
   the fixup sha IS the cascade instruction; no separate ledger.
4. **one cascade** - at the end, ONE `git rebase -i --autosquash
   --update-refs master` walks the whole climb: autosquash drops each
   fixup next to its target, --update-refs moves every rel/ branch
   pointing inside the range. conflicts are the price of honest
   per-era releases; the escape hatch is "let it ride to the tip".
5. **ship oldest-first** - verify the replay, fast-forward master, cut
   the tags in order, make the releases in order, archive the
   pre-rewrite head.

the soundness argument, stated once: after the cascade, every version
tag is an ancestor of the next, all the way to v1.0.0 and master. no
divergent release branches, no maintenance lines, no tag that lies
about its content. the zero-risk alternative (fixes ride the next era,
no rewrite) is strictly easier and strictly less honest: the era tags
would ship code the review already judged wrong.

the one discipline that keeps the cascade cheap: **cascade fixes,
never redesigns.** a wrong message, a missing guard, a test gap, a
missed rooting - these cascade. a shape you would build differently
today is v1.x work and rides the tip. if a fix needs more than about a
screen of diff to apply three eras back, that is the signal it is a
redesign in disguise.

## section 2 - phase 0: prepare (an hour, before any reviewing)

tools and truth first:

```
# --update-refs needs git >= 2.38
git --version

# map what exists; fill section 0's table with real shas as you go
git fetch origin
git log --oneline --graph --decorate master classes-staging coro \
  modules-staging | head -80
git tag --list        # expect v0.1.0 and nothing from the climb

# THE gate: the whole plan's "master never moved" property
git merge-base master modules-staging   # must print master's tip sha
```

if the merge-base is not master's tip, stop and reread section 7's
"master moved" entry: the review still works, the ship step just stops
being a fast-forward and deserves its own thinking.

backups, in order of how much they will save you:

```
# the undo point, one per lane that matters (minimum: the rc head)
git branch backup/pre-review modules-staging

# rerere: the cascade's best friend. when the same conflict shape
# appears at two insertion points, or when you redo the cascade after
# an abort, git replays your resolution after you solved it once.
git config rerere.enabled true

# the bundle: everything, dated, one file, restorable anywhere
git bundle create ../oklang-pre-review-$(date +%F).bundle --all
```

pin the era cuts. the lanes exist as branches; the cascade needs one
branch per VERSION so --update-refs has something to move (it moves
branches, never tags - which is exactly why the tags wait):

```
git branch rel/v0.5 classes-staging    # the 0.5.0 cut
git branch rel/v0.6 1672c21            # the coros commit (patch 38)
git branch rel/v0.7 coro               # the Epigone tip (39-42)
git branch rel/v0.8 973bcd0            # the 0.8.0 cut point
# rel/v0.9 and rel/v1.0 get pinned when the wave and the rc exist

# the redo insurance: the cascade will move rel/*, so freeze their
# pre-cascade positions now. redo becomes: reset, re-run.
for v in v0.5 v0.6 v0.7 v0.8; do git branch backup/rel/$v rel/$v; done
```

(the pin rule: the era's tip AS REVIEWED. if the lane grew a few
commits after landing, pin where the version's content is decided -
the review and its fixups move the pin anyway, so precision here is
cheap.)

worktrees, one per thing you want alive at once:

```
git worktree add ../ok-review modules-staging   # fixes happen here
git worktree add ../ok-era-v0.5 rel/v0.5        # reading happens here
```

reviewing an era does not mean living in it. read the diff anywhere;
spin up an era worktree only to BUILD that era - run its gates,
reproduce a bug at that commit, feel the history.

## section 3 - phase 1: review (the multi-day part)

per era, oldest first. the reading order inside an era:

```
# the commits, then the map, then the diff by area
git log --oneline master..rel/v0.5
git diff --stat master...rel/v0.5
git diff master...rel/v0.5 -- src/
git diff master...rel/v0.5 -- include/ tests/
```

three-dot (`A...B` = merge-base to B) is deliberate: wherever the
lanes diverged or were applied out of order, it keeps the era's diff
honest - what B's era added since the common ancestor, not the
symmetric difference of two arbitrary tips. for the middle eras, diff
against the PREVIOUS era's pin, not master:

```
git diff rel/v0.5...rel/v0.6 -- src/    # what the coros era added
```

the map for what to look at: the guide's section 9 risk table first
(the mount window's rooting, the capability ratchet, the ring's five
lanes, the ABI compat rule, the resolution ladder, the weak interner),
then the guide's era sections, then the drafts for the law when the
code and the guide disagree (that disagreement is itself a finding).

notes: keep a REVIEW-NOTES.local.md at the repo root, untracked via
`echo "REVIEW-NOTES.local.md" >> .git/info/exclude` (local-only ignore
- it never rides a commit). one line per finding:

```
okparser.c:812  brace matching drops the span   fixup v0.5
okmodules.c     alloc in error path rides libc  fixup v0.8
classes draft   operator dispatch table reads stale after freeze
                -> redesign, rides tip, v1.x
```

and the move that makes the whole plan work - fix while hot. you are
reading era E, the fix is obvious, the era's code is in your head:
walk to the ok-review worktree (the tip), write the fix THERE, and
commit it against the era:

```
# in ../ok-review, at the rc tip:
git commit --fixup=<the offending sha inside rel/v0.5's history>
```

the fixup discipline:

- fixups are SMALL: one logical fix per commit, tests in the same
  commit (the culture, unchanged).
- target the sha that INTRODUCED the problem when you know it (the
  fix lands next to its cause); target the era tip when you do not
  (the fix still lands inside the era - the pin is above it).
- plain commits whose message names the era ("Cascade: v0.7+") work
  too; the fixup spelling just lets autosquash do the sorting.
- fixes that only make sense at the tip (the delta blue pub-fields
  rewrite, anything the 0.9 wave itself finds in its own code) are
  ordinary commits. no fixup, no cascade.
- run the gates at the tip as you go - every fixup lands on a tree
  that must stay green. the tip is the only place the full matrix is
  meaningful during review.

build the eras you distrust (the guide's risk ranking is the priority
order). the per-era bar at this stage: it builds, and its own e2e
passes at its pin, plain and aggressive (aggressive gc is where
rooting sins always show). the full matrix comes after the cascade.

draft the release notes as you finish each era - the era is fresh in
your head now, and release day becomes boring:

```
git shortlog master..rel/v0.5           # the raw material
git log master..rel/v0.5 --oneline
# then write notes/v0.5.0.md: the era's story in user terms
```

## section 4 - phase 2: the cascade (one sitting, undisturbed)

preconditions: working tree clean, every fixup committed, the section 2
backups exist, rerere on, nobody else is pushing to the lanes.

```
git switch modules-staging      # the rc head (or the wave lane)
git rebase -i --autosquash --update-refs master
```

what the todo will look like: every pick from master to the tip, each
fixup already sorted next to its target by autosquash, and lines like
`update-ref refs/heads/rel/v0.5` sitting exactly where those branches
point today. the cascade happens as written: a fixup listed above an
era's update-ref line is CONTAINED in that era's cut.

before saving, two eyeball checks:

- every fixup sits ABOVE (before) the update-ref of its era. autosquash
  handles this when the target is inside the era; if you retargeted
  something by hand, this is the line to re-check.
- if phase 0's graph showed MERGE commits inside the climb, add
  `--rebase-merges` to the command. the todo gains label/reset lines;
  same rule, move picks only within their segment.

conflicts, with honest expectations:

- a fix written against the tip may not apply cleanly three eras back.
  if the resolution is small, make it. if it is not, move the pick
  BELOW the next update-ref line - the fix rides the next era, and
  containment degrades gracefully. never force a bad fit.
- `git rebase --continue` cycles through them; rerere quietly learns
  each resolution as you go.
- `git rebase --abort` restores the world exactly as it was. the
  cascade is retryable without limit, because nothing public has moved.

when it completes, the lanes and every rel/ pin have already moved
(that was --update-refs). eyeball the shape before touching anything
else:

```
git log --oneline --graph --decorate | head -40
```

## section 5 - phase 3: verify the replay

the principle: the cascade is a content no-op on the tip. fixups moved
from the tip into the eras, so the tip's TREE must come out identical:

```
# the receipt. expect: empty. any hunk here is a conflict resolution
# that changed its mind mid-cascade - read it, own it, or redo it.
git diff backup/pre-review modules-staging
```

then the full matrix at the rc head, exactly like a landing: e2e x7
(plain, threaded, asan, aggressive, asan-aggr, threaded-asan,
threaded-asan-aggr), unit, embed, capi x5, delta blue correct at
1.40654e7.

then the per-era spot pass - the cascade's one sneaky failure mode is
the SEMANTIC conflict (a fix that applies cleanly but means something
different in the old tree), and only the eras' own gates can catch it:

```
for v in v0.5 v0.6 v0.7 v0.8; do
  git worktree add ../ok-verify-$v rel/$v || break
  ( cd ../ok-verify-$v && ./scripts/build_ok.sh plain \
      && ./scripts/run_e2e_full.sh plain \
      && ./scripts/build_ok.sh aggressive \
      && ./scripts/run_e2e_full.sh aggressive ) || { echo "FAIL $v"; break; }
  git worktree remove ../ok-verify-$v
  echo "OK $v"
done
```

(the build/e2e invocations above are the shape - use the real ones. an
era failing here means a fixup hurt it: either fix it inside that era
and re-run the cascade from the backups, or move that fixup down a
era. section 7 has the redo recipe.)

## section 6 - phase 4: ship

nothing below happens until phase 3 is green. order matters.

```
# 1. master. the rewritten climb roots at master's tip, so this is an
#    ordinary fast-forward push - the only "merge" the repo ever needs:
git push origin modules-staging:master

# 2. the lanes on codeberg moved (the rewrite changed their shas), so
#    they need force - with the lease, always:
git push --force-with-lease origin classes-staging coro \
  modules-staging

# 3. the archive tag, so the pre-rewrite head stays fetchable forever
#    (old bench run-infos pin those shas; a force-push orphans them):
git tag archive/rc-pre-review backup/pre-review
git push origin archive/rc-pre-review

# 4. the version tags, in order, from the moved rel/ pins:
git tag -a v0.5.0 -m "oklang 0.5.0 (Citadel)"         rel/v0.5
git tag -a v0.6.0 -m "oklang 0.6.0"                   rel/v0.6
git tag -a v0.7.0 -m "oklang 0.7.0 (Epigone)"         rel/v0.7
git tag -a v0.8.0 -m "oklang 0.8.0 (O Resolution)"    rel/v0.8
git tag -a v0.9.0 -m "oklang 0.9.0 (Reconciliation)"  rel/v0.9
git tag -a v1.0.0 -m "oklang 1.0.0 (Celestial)"       rel/v1.0
git push origin v0.5.0 v0.6.0 v0.7.0 v0.8.0 v0.9.0 v1.0.0
```

(the v0.9.0 and v1.0.0 lines exist once the wave and the review are
done; if the plan compresses, the ORDER is the contract, not the
count. tags are created fresh - nothing from the climb was ever
tagged, so there is nothing to move or force.)

5. the releases, oldest first, on codeberg (web UI per tag, or tea):
   title "oklang 0.5.0 - Citadel", body from notes/v0.5.0.md - the
   era's story in user terms, the receipts (e2e at landing is the
   flex; the matrix at the re-cut tip is the proof), and the upgrade
   notes an embedder cares about (the ABI table for 0.8.0, the
   deprecation windows for 0.9.0).

aftercare, the same day (all cheap, all easy to forget):

- **collab repo**: dispatch the bench workflow at master - it finally
  builds current code again, and the run re-baselines the whole suite
  (benches.yml's oklang_ref input was built for exactly this day).
- **tree-sitter-oklang**: the standing grammar debt (the hardcoded
  escape set beyond \n, the string work, varargs, two-binding for-in,
  classes details, coros, pub on class-body let - plus whatever the
  0.9.0 strings round adds: f-strings, m-strings). tag the grammar
  release alongside v1.0.0; the neovim plugin rides it. Q's flank, per
  the patch-44 supersession precedent.
- **retire the lanes** that are now master, after a week of nobody
  missing them: `git push origin --delete classes-staging` etc., local
  `git branch -d` to match. keep: master, the next wave's lane, rel/*
  until v1.0.0 has been out a while, backup/* and archive/* forever.
  tidy is pre-retired (session 42): delete it any time - coro owns
  everything it ever pointed at.
- **the docs wave**: mark the release plan EXECUTED in ROADMAP and
  DECISIONS; this runbook moves to archive/ with the release receipts
  appended.

## section 7 - when it goes wrong

- **the cascade conflicts badly**: `git rebase --abort`. either retry
  with the offending fixup moved down an era, or turn it into a plain
  commit at the tip (it rides v1.0.0 only - which still fixes every
  version that matters going forward; the pre-1.0 releases are
  waypoints, not LTS lines).
- **a fixup will not apply in any era**: same answer - it rides the
  tip. containment is best-effort BY DESIGN.
- **an era's gates fail after the cascade** (the semantic conflict):
  fix it inside that era as a new fixup, then redo the cascade from
  the frozen positions:
  ```
  git branch -f modules-staging backup/pre-review
  for v in v0.5 v0.6 v0.7 v0.8; do git branch -f rel/$v backup/rel/$v; done
  # add the new fixup at the tip if it is not there yet, then:
  git rebase -i --autosquash --update-refs master
  ```
  rerere makes the second pass dramatically cheaper than the first.
- **master moved** (someone pushed to it mid-review): the fast-forward
  is gone. re-check with `git merge-base master modules-staging`. the
  review is unaffected; the ship step becomes a merge or a force
  decision that deserves its own conversation - have it before
  pushing, not after.
- **"i just want to look at v0.5.0"**: `git switch --detach rel/v0.5`
  (before tags) or `git switch --detach v0.5.0` (after).
- **the sandbox ate the world**: the collab repo's WORKLOG plus the
  bundle from section 2 reconstruct everything; recovery protocol as
  usual.

## section 8 - the cheat sheet

```
# prepare
git fetch origin && git config rerere.enabled true
git branch backup/pre-review modules-staging
git bundle create ../oklang-pre-review-$(date +%F).bundle --all
git branch rel/v0.5 classes-staging
git branch rel/v0.6 1672c21 && git branch rel/v0.7 coro
git branch rel/v0.8 973bcd0
for v in v0.5 v0.6 v0.7 v0.8; do git branch backup/rel/$v rel/$v; done

# review, per era (oldest first)
git log --oneline master..rel/v0.5
git diff master...rel/v0.5 -- src/ include/ tests/
#   spot a fix -> worktree at tip -> git commit --fixup=<era sha>
#   finish era -> write notes/v0.5.0.md

# cascade (once, at the end)
git switch modules-staging
git rebase -i --autosquash --update-refs master
git diff backup/pre-review modules-staging     # MUST be empty

# verify
full matrix at the tip + per-era plain/aggressive spot pass

# ship
git push origin modules-staging:master
git push --force-with-lease origin classes-staging coro modules-staging
git tag archive/rc-pre-review backup/pre-review
git tag -a v0.5.0 -m "oklang 0.5.0 (Citadel)" rel/v0.5   # ... per era
git push origin v0.5.0 v0.6.0 v0.7.0 v0.8.0 v0.9.0 v1.0.0
# releases on codeberg, oldest first; then the aftercare list
```

## section 9 - what this runbook deliberately does not do

- no merges into master: the climb lands as one line (the lanes were
  stacked by construction; if reality disagrees, phase 0's graph says
  so on day one, and --rebase-merges keeps the rewrite honest).
- no release branches or maintenance lines: every tag is an ancestor
  of the next. a hypothetical v0.5.1 hotfix release is cut from the
  v0.5.0 tag plus cherry-picks - a different, smaller runbook, and one
  the pre-1.0 waypoint releases should never need.
- no signed tags: annotated only, matching the repo's stance; revisit
  if distribution ever demands provenance.
- the collab repo and tree-sitter are satellites: this runbook touches
  only codeberg oklang. their day-one tasks live in the aftercare.

## appendix - the rulings this runbook stands on

- the release plan (RULED, Q, session 40's round): no release now;
  finish v1.0-rc, review and amend, then release the debt v0.5 -> v1
  as its own releases. recorded in ROADMAP-0.9.0-draft.
- fixups + one autosquash/--update-refs pass (Z's mechanics, accepted
  with the plan): the pain budgeted once, tags born after the rewrite,
  backup lanes + the archive tag for the SHAs the bench run-infos pin.
- the era table (the guide's section 0, verified at 973bcd0): Citadel,
  coros, Epigone, O Resolution, and the follow-ups 51-52 that closed
  the 0.8.0 lane.
- tidy out of the pipeline (RULED, Q, session 42): the branch stalled
  at the coros commit and was superseded - coros carries everything.
  0.6.0 pins the coros sha, 0.7.0 pins coro's tip, tidy joins no
  command and no release.
- cascade fixes, never redesigns (Z's discipline, this round): the
  boundary between "wrong then, wrong now" and "i would build it
  differently today" - the first cascades, the second rides the tip.
