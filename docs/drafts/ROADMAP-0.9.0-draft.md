# ROADMAP 0.9.0 — the last pre-v1 wave (draft, round one folded)

status: DRAFT, round one complete (2026-11-04), round two folded. 2026-11-05
(session 48): P6 ROUND ONE delivered (patch 62: OP_TAILCALL + the
member-store fix; the pub-fields bench rewrite re-baselined delta blue
from 1.93x to 1.35x of wren). 2026-11-05
(session 47): P5 landed on reconciliation (Q's 4ad2cbd) carrying the FIRST
DRIFT in five landings — the invalid-utf-8 corpus seed mangled to U+FFFD
by the landing medium, verdict silently flipped; patch 61 (the corpus
integrity lane) answers the class: gen.sh + fnv1a64 checksums + load-time
verification. 2026-11-05
(session 46): P1-P5 LANDED (patches 53-60; P5 delivered whole — hardening +
fuzzer + cross-check + REPL, with the fuzzer's four pre-existing bug finds
fixed), the PORTABILITY LANE recorded (pure c99 at compile time, lua-shaped,
deliberately deferred per Q's ruling). 2026-11-05
(session 45): P1-P4 LANDED (patches 53-59), the series CONDENSED per
Q's ruling — "now also that the session don't cut off as aggressively
you can condense the remaining patches as appropriate": one patch per
workstream, whole workstreams per session when the session holds. the
modules wave is
complete (patches 43-52, e2e 510x7); this draft shapes the last pre-v1
version. Q's original framing: "we literally have nothing left, only
minor stuff (utf8, std expansion pack, and tooling (cli already
partially made), and more testing and parser harden + fuzz, and
optimization, so i think all those go in one version, maybe a session
or 2". the OPEN items were collected for one round; Q's message
answered most of them. round two (session 41) closed the rest: len,
the codename, property tests, the perf target, the GC call, and the
std door are all RULED below, plus ${} as the interpolation delimiter.
the git mechanics for the ruled release plan are now their own doc:
RELEASE-RUNBOOK.md (the review & release runbook, v0.5.0 Citadel to
v1.0.0 Celestial).

codename: Q's two candidates — "Epilogue" (the opeth track) or
"Reconciliation" (xanthochroid's closing "towards truth and
reconciliation" — bookending "Etymos", the first release's nod to their
world). Z's endorsement: **Reconciliation**. it is not just the arc
closing, the release *is* one — it reconciles five versions of
accumulated debt, the review, the branches, into the v1 line. "Epilogue"
stays in the pocket (it is good — it is just generic next to a name
that does actual work). RULED (Q, round two): **Reconciliation** —
"since you liked Reconciliation more, and also i was truly leaning
towards it (noticed that? lol) keep it, it is a perfect fit."

## the release plan (RULED, Q — the sequencing changed)

the 0.8.0 cut no longer gates the wave. "i will not release now, we
will finish v1.0-rc then i do review and amend what needed then release
are the debt (v0.5 -> v1) as their own releases". so:

1. the wave runs to **v1.0-rc** on the staging lanes. no mid-wave
   release, no master merge, the review debt accrues on purpose.
2. the review happens on the rc content: `git diff` walks between the
   era staging branches (three-dot `A...B` — merge-base to tip — keeps
   the eras honest wherever branches diverged), plus the guide as the
   map.
3. the amendments slot into their eras: fixup commits on the rc head
   (`git commit --fixup=<era sha>` while reviewing, or plain commits
   labeled per era), then ONE `git rebase -i --autosquash
   --update-refs v0.4.0` walks the whole history, drops each fixup onto
   its target, and moves every era branch that points inside the range.
4. the debt releases cut from the rebased era heads: v0.5.0 (Citadel),
   v0.6.0, v0.7.0 (Epigone), v0.8.0 (O Resolution), v0.9.0
   (Reconciliation), then v1.0.0 (Celestial) — each its own release,
   changelog from its era's log. [round-two correction: round one had
   the codenames shifted one slot late; the guide's era table is the
   authority.] the mechanics live in RELEASE-RUNBOOK.md now: fixups at
   the tip while reviewing, ONE autosquash + --update-refs cascade,
   the replay verified (the tip diff must come back empty), master
   fast-forwarded, tags born after, releases oldest-first, and the
   archive tag keeping the pre-rewrite SHAs fetchable for the old
   bench run-infos.

soundness (Z's read: yes, the plan is sound — the mechanics, refined):

- `--update-refs` (git >= 2.38) moves refs/heads/* inside the range;
  **tags never move**. nothing is tagged until after the rebase —
  master sits at v0.4.0, so this is free. just do not tag early.
- editing *inside* a year-long interactive rebase todo is the painful
  part; fixup + autosquash is the same plan with the pain budgeted
  once. conflicts are the price of honest per-era releases: a fix
  written against the final tree may not apply three eras back. bounded,
  small fixes only.
- backup before: `git branch backup/pre-review <rc-head>` per lane.
  keep an `archive/rc-pre-review` tag after the force-push so the SHAs
  pinned in old bench run-infos stay fetchable (a force-push orphans
  them otherwise).
- the rebased head re-runs the full gate matrix (e2e x7, capi, unit,
  embed, delta blue) before any tag is cut. the replay should be a
  content no-op; the matrix is the proof.
- the zero-risk alternative (fixes ride the *next* era, no rewrite)
  makes the era tags lie about their content. the rebase plan is the
  honest one. endorsed.

`ok --version` (+ `ok -v`, and the `ok version` spelling): RULED in —
one compiled stamp, single source of truth: the version, the abi
(major.minor, the patch-52 encoding), and the lane (mid-wave it reports
the rc pre-release, e.g. 0.9.0-rc.1). the bench workflow's tolerant
`ok --version 2>/dev/null || true` line drops its tolerance the same
patch. `ok --dump` already exists — struck from the list.

## the workstreams

### 1. UTF-8 (the codepoint-first amendment)

- RULED (Q, amending the earlier fold): **codepoint-first,
  python-shaped — "thats for iteration + index"**. `for c in s`
  iterates codepoints; `s[i]` is the i-th codepoint (a fresh
  one-codepoint string), not the i-th byte. the ranged-for grows the
  str's own hidden shape NOW (a utf8 cursor, advance-per-step); the
  arr/map shapes are untouched.
- **len — RULED (Q, round two): codepoints**. "len -> codepoints, as
  you said, with maybe later optional byte indexing/size via methods,
  language surface should stay coherent with itself." both counts
  cached in the interned header (strings are immutable — the cache is
  free), byte length behind an explicit `s.bytes()` later if a real
  consumer appears. the `s[s.len()-1]` trap dies.
- **the hidden shapes stay forever** (RULED, Q's instinct — "it is just
  branching on start right so it won't hurt right?" — correct): one
  type-check at loop entry, then the monomorphic inline path. this is
  the specialization trick CPython's FOR_ITER grew in 3.11, and it is
  why ok's ranged-for already outruns lua's pairs/ipairs. the contract
  (tested): the hidden path and any generic fallback are
  observationally identical — a conformance test wraps the builtins in
  user classes, forces the generic path, and compares.
- **the iterator protocol: DEFERRED past v1** (Z's recommendation to
  Q's "do you recommend doing iterator protocol now?"). reasons: (a)
  adding a fallback dispatch later is purely additive — go shipped
  2012-2024 without one and nobody calls go 1.0 incomplete; (b) the
  hidden shapes cover every builtin, so std needs nothing from it; (c)
  the wave is already full. where it lives when it lands: **a VM-level
  convention like len — not std, not std-zero, not a module**. std.proto
  is rejected as a name (it reads "protocol" and "prototypes" both, and
  the VM must call the hook without anyone importing anything);
  std.iter is where *adapters* would live post-v1. the shape, when it
  lands: **class-based** (an iterator object holds the state), `iter()`
  → iterator, `has_next() → bool`, `next() → value`. no sentinel
  design: `null` is a legitimate value in ok arrays, so a null-sentinel
  next() is the classic ambiguity; has_next/next sidesteps it without
  exceptions. **the names `iter`/`has_next`/`next` are reserved in the
  v1 docs** so the eventual protocol collides with nobody.
- **the lexer: keep the reverse-check** (RULED, Q — "i think this is
  better approach than ascii only identifiers"). the blocklist shape
  stays: an identifier is anything the language's own punctuation does
  not claim, bytes >= 0x80 match no case label and ride through, no
  tables. the draft's "ascii identifiers v1" line is struck. the
  missing half is the gate: **one whole-source validation pass at the
  front door**. the C++ header (`ok::utf8`) ports to C99 — `ok_utf8`,
  with the FIXME resolved into a contract: advance() returns 0 on
  invalid (callers on validated regions assert), validate() is the only
  entry that may see bad bytes, failure is a parse error carrying the
  byte offset. the header's checks are already right (overlong C0/C1,
  E0 80-9F, surrogates ED A0-BF, F0 80-8F, > F4, F4 90+). the gate also
  rejects: NUL anywhere, the bidi controls (U+202A-202E, U+2066-2069 —
  the trojan-source hole, six codepoints), C0 except \t \n \r. run at
  compile entry, REPL line ingest, and module source loads.
  identifiers then accept any well-formed non-ascii codepoint and the
  lexer never learns unicode exists. (a future XID tightening, if ever,
  lands warning-first.)
- runtime strings (fs reads, env, argv): the Go-style lossy ruling
  stands — U+FFFD per malformed sequence at the codepoint operations,
  never a crash, never rejection at construction.
- the ONE column function ruling stands: one shared codepoint-column
  routine serves diag spans, the disassembler, and any future tooling.
- `\u{...}` escapes ride the string-scanner rework (P2/P3 share it).

### 2. the std expansion pack (the kills)

- RULED (Q): **std.str, std.map, std.array are killed** — "all 3 are
  std-zero classes and all their stuff is already encapsulated in the
  class". the class surface is the canonical surface. one nuance
  carried in: str carries NOTHING today (the natives-as-classes table:
  array len/push/pop/remove; map len/remove/get; str/function/num/
  bool/null empty) — so the planned string toolkit (trim/split/join/
  contains/starts_with/ends_with/index_of/upper/lower/repeat, the
  byte/codepoint pair) lands as **methods on the str class**, the same
  lane array/map use, not as a std module. the single-surface principle
  holds.
- RULED (Q): **the free spellings retire in the wave** — "native free
  functions get to retier here (len, pop, push, remove) the class
  variant are the canonical". per the window discipline: the
  deprecation notices open with 0.9.0's docs, the removal lands with
  v1.0.0 (Q may compress to a hard cut in 0.9.0 — pre-1.0 owes nothing
  — but the window keeps the culture). **len survives**: the hidden
  shapes lean on it ("len is still needed for our fake iterator") and
  the module read API's array walk is len+index shaped.
- std.math opens as drafted: the cmath surface, deterministic. plus
  **integer division** (RULED, Q, round two: "we are f64 and integer
  div is needed, maybe we put in std math since we reserve // for
  comments"): `math.idiv(a, b)`, trunc toward zero, error on a zero
  divisor, `math.floor(a / b)` composes when floor semantics are
  wanted. pairs with the modulo story; no operator (the // slot is
  comments forever).
- **the sys door's final shape (RULED, Q, round two)**: the drafted set
  is good enough — "we can always add stuff later". the additions, all
  std.sys: `sys.args()` (the argument vector after the program name),
  `sys.name()` (the process name), and a stderr line write
  `sys.err(...)` (no formatting opinions; placement is Z's default —
  if std.io ever grows past print, stderr moves there and sys.err
  stays as the sugar).
- **fs/env/clock placement (Q asked "what is the difference between os
  and sys?")**: the convention — `os` names the operating system's
  services (fs, env, process — python's os, wren's os); `sys` names the
  running program's system context (env, clocks, exit — python's sys
  spirit). ok already splits fs out, and the remainder (env, clock,
  exit) is exactly the sys bucket, so: **std.sys** (env get/set/unset/
  keys, clock + monotonic, exit) and **std.fs** (files, dirs, paths,
  cwd). both HOST-gated (D16) as drafted. when process spawn exists
  someday it is either std.sys.spawn or its own std.proc — the round
  can leave that open; the cap answer below is the same either way.
- **shells (Q asked "do they bring additional caps? or OK_CAP_HOST
  covers them?")**: RULED as doctrine, no new code — a shell/process
  spawn is **arbitrary code execution, not data-plane**, so it NEVER
  falls under OK_CAP_HOST. ok_specs.h's own family comment already says
  it: LOAD is arbitrary code in-process, BUILD is arbitrary code
  out-of-process, HOST is data-plane leakage. when std.proc exists
  (post-v1) it takes its own **OK_CAP_PROC**, sibling of the BUILD
  family, implied by no umbrella. the module itself is not in 0.9.0.
- the caps mask stays monolithic HOST through 0.9.0 (D16 unchanged).
  the fs/env/clock SPLIT (an embedder denying env but keeping fs) is a
  1.x refinement the semver window legally allows — append-only bits,
  old embedders unaffected — deferred until a real embedder asks.
  recorded so the v1 charter can state the boundary.
- the print STATEMENT deprecation window opens as drafted (one release
  of warning docs; removal is v1's call); std.io's own formatting
  replaces the %g default behind the same window; the shipping doctrine
  stands (hot core in C, leaf modules as ok source embedded in the
  registry, nothing of ok's own ever loads through OKPATH or dlopen).

### 3. tooling (the CLI grows up, no more)

- `ok version` + `ok --version` + `ok -v`: in, with the compiled stamp
  (version + abi + lane). P1.
- `ok --dump`: DONE — exists. struck.
- **`ok create <name>`** (RULED, Q's rename of `ok new` — "in cli i
  like this, yes we should make it"): scaffolds project.ok +
  src/main.ok + tests/ + a .gitignore (.okbuild/). five lines of C
  standing on project.c.
- **`ok test`: DROPPED** (RULED, Q's pushback — "tests are separate
  thing from the language cli, and it also signlas to users that this
  is a testing framework for their own code not to test the language
  which is weird!?" — Z concurs). the language's own suite stays the
  repo's e2e harness: internal tooling, not product. a user-facing test
  story is one coherent post-v1 feature (std.testing + a runner, when
  there is an assertions surface to run).
- **the test-path fix (Q's flag)**: the project/e2e harnesses stop
  reaching for `../../ok` — an **OK_ROOT** env var (set by the runner
  script and CI, fallback to the compiled-in prefix) points at the
  oklang checkout; the harnesses read it, the relative paths die. rides
  P1.
- **the REPL grows up (NEW, Q's list)**: raw-mode line editing
  (ctrl-a/e, ctrl-left/right word jumps, ctrl-u/k/w), history (up/down
  + persisted with a size cap), **auto-print of lone expressions** (the
  repr — strings quoted — instead of `print expr`; suppression by
  trailing `;`), multi-line continuation on unclosed braces with a
  `...` prompt. strictly POSIX termios, CLI layer only — libok stays
  out of it ("since our cli is strictly posix now, it is acceptable");
  windows-friendliness stays a separate, later project.
- ok fmt / ok doc: unchanged — v1.x, itch-driven per the standing
  doctrine.

### 4. testing + parser hardening + fuzz

- Q's framing recorded: "the parser overall is so fragile, and it is
  more likely to segfault and break and corrupt memory than almost all
  other parts of the language so it deserves good look at as a whole,
  and then fuzzing tests". so the order is **harden first, fuzz
  second** — the fuzzer finds bugs, the hardening removes bug CLASSES:
  - a depth guard: an explicit nesting limit with the clean "nesting
    too deep" diagnostic instead of C-stack overflow — the segfault
    class.
  - arena ownership: every parse node born in the compile arena, error
    paths never free individual nodes — the arena dies wholesale (the
    use-after-free/double-free class).
  - every peek/eat bounded through the one helper, no raw lookahead
    indexes — the out-of-bounds class.
  - recovery syncs at statement boundaries only; never continue
    mid-expression with partial nodes (that is where corruption
    breeds).
- the zero-dep fuzzer stands as drafted (the stance): corpus-driven,
  deterministic seed loop, byte mutations over the fixture corpus +
  synthesized inputs, feeding lexer/parser/compiler; the bar is
  no-crash + no-asan-trip + every diagnostic carries a location; corpus
  snapshots committed so CI-style runs are reproducible. the natives
  ABI boundary (a hostile module descriptor through the patch-48
  loader) joins its menu later.
- the tree-sitter cross-check corpus grows with the fuzz findings (the
  patch-16 discipline: real parser vs grammar, drift is a bug).
- e2e growth: the 510 target keeps ratcheting; each workstream above
  lands with its tests in the same commit (the culture, not a change).
- property tests: RULED (Q, round two) — post-v1, not urgent. the
  patch-19 pattern waits for the v1.x lane.

### 5. optimization (delta blue is the meter)

- **the target number: RULED (Q, round two) — none.** "we never sat a
  hard number target before as it could be harmful, we just try to
  refine the arch as much as possible, all numbers are on the side
  really as long as we have a solid vm and solid design and good enough
  performance, we just try to make it the best version of itself." the
  bench suite is the compass (relative regressions), never a
  scoreboard; delta blue stays the meter.

the standing number: ok at 1.93x of wren on the allocation-shaped gap;
delta blue correct at 1.40654e7 through every gate since. the passes,
cheapest first, each measured alone against the WHOLE bench suite:

- **delta blue updates FIRST (RULED, Q)**: "the delta blue should be
  updated since the pub fields did not lang their and it was a
  bottelneck" — the bench predates patch 51; rewritten to pub fields it
  exercises the surface the language will actually ship, and the
  re-baseline is honest. any bench leaning on the retiring free
  spellings updates in the same sweep.
- compare+jump fusion (Q's "no materialize"): conditions stop
  materializing booleans; the cmp operand rides the jump. the old
  branch_toggle gap (2.01x of lua at the register-vm milestone) is the
  same shape.
- allocation shaving on the hot paths: the delta blue profile's top
  offenders (string/number boxing churn in the inner loop); the
  interner (D20) already killed the lookup half. NOTE: the str hidden
  shape adds a known new churn — one fresh one-codepoint string per
  iteration — the shaving pass measures it (a small-string fast path is
  the mitigation if it shows).
- tailcalls (RULED, Q's "one new op for tail calls that will be super
  useful"): the explicit OP_TAILCALL as drafted — tail position reuses
  the frame, O(1) stack, deep recursion stops being a cliff. two
  cautions carried in: stack traces lose the elided frames (the debug
  lane keeps fidelity if Q wants it), and arg-slot capture (upvalues
  over parameters) must be settled BEFORE frame reuse — copy the
  captures or refuse the tail call, never reuse a frame whose args are
  still captured.
- the cheap-wins list from the perf sessions rides the same patch,
  cheapest-first.
- **the bench workflow (Q's ask — "a workflow in the collab repo to
  allow building against oklang other than master... thus we can watch
  perf before commiting to release")**: the ref knob ALREADY EXISTS —
  benches.yml is workflow_dispatch with an oklang_ref input (default
  master, clones codeberg at that ref). what is missing: (a) a bridge
  so pushes to a staging lane FIRE the collab run automatically (a
  three-line dispatch workflow in the oklang repo, or a schedule
  polling the lane head — manual dispatch only, today); (b) the clone's
  `--depth 1 --branch` takes branches and tags but not raw SHAs — fine,
  documented; (c) a baseline column in the comparison (the lane run vs
  the last master run; plot.py's overview already carries the shape).
- **the GC question, framed** (Q: "should we make it incremental/
  perhaps even generational pre-v1 (since we are dropping a lot of
  powerful stuff pre v1, so the language feels complete at release?)"):
  Z's recommendation — **incremental: yes, if the wave's measurement
  allows it** (slice budget + the write barriers on array/map stores
  whose headroom the classes lane kept on purpose; the stop-the-world
  pause is the last latency spike, and "feels complete" is a latency
  argument, not a throughput one — the REPL and embedders feel pauses,
  benches mostly do not). **generational: no — post-1.0, explicitly**:
  it is a throughput optimization (two spaces, promotion policy,
  barrier cost on every store), and the allocation-shaving pass may
  shrink the very churn it feeds on; revisit with post-v1 profiling.
  the decision stays measurement's (dorg3-style dogfood, not
  synthetics): attempt incremental in P7, keep it behind the bench bar,
  defer with data if it trips.
  RULED (Q, round two): **incremental in v1 if time helps, generational
  post-v1** ("it time helps yes incremental v1 then post v1 perhaps
  generational"). P7 stays the attempt slot; the bench bar stays the
  judge.

### 6. C99 + the host-abstraction audit (NEW, Q's ask)

"we should make sure we are c99, and all host stuff are behind
abstractions, and we make sure that we respect that in every place
(allocators and fs, and dylib stuff) i am sure some have gone
unnoteced, and finally we get to kill all warnnings from before":

- the compile gate: `-std=c99 -pedantic -Wall -Wextra -Werror`, gcc +
  clang in CI (no -std flag is pinned today — the default drifts with
  the compiler). feature-test macros centralized in ONE header so POSIX
  stays opt-in per layer: libok's core compiles freestanding-shaped,
  the loader/fs/time layers opt in deliberately.
- the audit: a grep sweep for direct libc outside the abstraction
  layers — the malloc family, fs (fopen/open/read/write/close), time,
  env, the dylib family, and the printf family inside libok
  (diagnostics go through the diag core, not fprintf). libok routes
  everything through ok_mm / ok_dylib / the ok_fs + ok_time layers the
  fs/env/clock workstream opens anyway — the audit and the natives
  share a patch (P4). the CLI may use POSIX directly (its lane, strictly
  posix per the REPL ruling) but gets the same -std gate.
- warnings: one sweep kills the backlog; -Werror in CI keeps them dead.

## explicitly NOT in 0.9.0 (the standing vetoes, restated + the round's additions)

scheduler/async (casio is a separate project, forever), JIT, threads,
int scalars, regex, package registry (M11 — possibly forever), dynamic
import/importlib (M4's later-lane), hot reload, debugger v0 and LSP
(itch-driven, post-v1), serialization + okc (post-v1), module unloading
(the lua lesson), bytecode cache (compiles are fast until a real
project hurts). added this round: **the iterator protocol** (post-v1,
names reserved), **generational GC** (post-1.0), **std.proc/shells**
(post-v1, OK_CAP_PROC doctrine recorded), **ok test + std.testing**
(post-v1, one coherent feature).

## v1.0.0 "Celestial" — the shape after this wave (for context, not scope)

the contract freeze: "a stranger can embed oklang and trust what ships
won't break them" (ROADMAP-v1's definition). the checklist this wave
should leave ready: the charter (what the 1.0 promise covers: the
script surface, ok.h, ok_module.h at ABI (1,0) — and what it explicitly
does not: bytecode, internals), plus two charter lines the round
earned: the caps doctrine (arbitrary-code caps are never implied by an
umbrella; HOST is and stays data-plane) and the reserved protocol names
(iter/has_next/next documented as forward promises). the docs
(reference, the embedding guide, the native-author guide — the guide's
§7 is the skeleton), distribution (tarball/flake/install target + the
lua-style amalgamation option), the deprecation windows closed (print
statement, %g, flat aliases), impl-defined behavior documented.
horizon: checklist is the clock, calendar is vibes.

## the patch series (re-densed after round one, condensed after P4)

one session each, gated by the full matrix every time:

- **P1**: LANDED (patch 53, Q's 057309c) — the version stamp + `ok
  --version` / `ok -v` + `ok create` + OK_ROOT in the harnesses + the
  c99/-Werror gate + the 530-warning sweep.
- **P2**: LANDED (patch 54, same landing) — the ok_utf8 port + the
  front-door validation gate (NUL / bidi / C0), codepoint-first
  iteration + index, the str hidden shape, len = codepoints (the open
  ruling resolved: Z's rec, Q's word, both counts cached), `\u{}`
  escapes + tests. one bug fixed in-patch: the for-slot call-window.
- **P3**: LANDED (patches 55-58, Q's f05445a + 54fd30f + d48edf4) —
  **f"" interpolation** (RULED round two: the delimiter is `${`
  followed by `}` — Q's proposal, Z's endorsement; `$` is unclaimed by
  the language, braces go back to being literal so JSON and friends
  need no escaping inside f-strings, `\${` is the literal
  dollar-brace, a `$` not followed by `{` is just a dollar, and plain
  "" never interpolates; full escaping at string level only; expressions
  nest with brace tracking so quoted strings inside interpolations
  work; NO format-spec mini-language — `:` after the expression is
  reserved and errors for now) + **m"" multiline** (literal newlines,
  same escape set, no dedent magic, `fm""` stacks; the unterminated
  error names the opening line) + the str method toolkit + std.math +
  the deprecation windows open; then the constants lane (pi, e) and
  the complete escape set (`\a \b \e \f \v \0`, `\xNN` to ASCII,
  octal refused; the `\u{}` braces KEPT — reason, not preference:
  variable width vs the BMP cap and surrogate pairs).
- **P4**: LANDED (patch 59, awaiting Q's hand) — fs/env/clock behind
  OK_CAP_HOST: std.fs + std.sys + the ok_fs/ok_time/ok_env one-door
  layers + the host-abstraction audit reroutes + sys.exit as the
  uncatchable OK_EK_EXIT lane (OK_EXITED + ok_exit_code public) + 21
  fixtures + the runners' `// expect exit:` lane + capi +9 (the
  sandbox lane got its first real HOST consumers).
- **P5**: LANDED on reconciliation as Q's 4ad2cbd — the front-door patch,
  condensed with P7's REPL round. the four hardening classes (the parse
  arena, the shared 200 depth funnel, the 25-report cap, nesting-aware
  statement-boundary recovery; bounded peek/eat closed by audit) + the
  zero-dep fuzzer with its committed corpus and the okdiag-sink location
  bar (FOUR pre-existing bugs found and fixed day one: the f-string NULL
  deinit, two deferred-init walks, the unary-plus abort) + the cross-check
  manifest lane (the tree-sitter sweep's diff target, grown with the fuzz
  findings) + the REPL round (termios raw-mode editing in the posix
  isolation zone, ctrl-left/right word jumps, history capped at 100,
  auto-print of lone expressions with `;` suppression, nesting-aware
  multi-line continuation, glob persistence - the repl invents nothing,
  `glob` survives the entry, plain `let` and imports are per-entry).
  the landing carried the FIRST DRIFT in five landings - the medium
  replaced the invalid-utf-8 corpus seed's bytes with U+FFFD, flipping
  its pinned verdict - answered by patch 61, the corpus integrity lane
  (gen.sh ascii-escape healer, fnv1a64 checksums verified at corpus
  load, crosscheck.sh the one regen door): apply 61, run
  `bash tests/fuzz/corpus/gen.sh`, commit the healed seed, and the
  trees are byte-identical again (simulated on 4ad2cbd: empty diff).
- **P6** (condensed with P7's GC question — the measurement patch):
  ROUND ONE LANDED (patch 62 + the collab-side rewrite): delta blue's
  pub-fields rewrite + re-baseline (1.93x/1.88x of wren -> 1.35x/1.33x
  — the accessor tax was ~30% of the benchmark; the rewrite also
  flushed out and fixed a pre-existing member-store miscompile),
  OP_TAILCALL (the frame hands over in place; a million mutual
  tailcalls ride one frame slot), the regen door's ordering fix.
  ROUND TWO REMAINS: compare+jump fusion pass 2 (the and/or comparison
  operands + negated conditions), allocation shaving against the NEW
  baseline, the bench bridge trigger + the baseline column — and the
  incremental GC attempt (or its measured deferral, with data) rides
  the same patch's measurement bar.
- P7 as a slot DISSOLVES into P5 + P6 above; the leftovers the next
  round decides stay collected in the worklog.

"maybe a session or 2" stays honest for what remains; the optimization
passes run as long as the profile says. everything lands on the
reconciliation lane (the working branch answers the codename question
in practice; the formal call stays Q's), same protocol.

## the survivors, closed (round two — nothing left OPEN)

- **len**: RULED codepoints (the `s[s.len()-1]` trap dies; `s.bytes()`
  later, if ever).
- **the codename**: RULED Reconciliation. Epilogue stays in the pocket.
- **property tests**: RULED post-v1, not urgent.
- **the optimization target**: RULED none — "the best version of
  itself"; the arch is the target, the numbers are telemetry.
- **the GC call**: RULED incremental in v1 if time helps, generational
  post-v1. P7 is the attempt slot, measurement is the judge.
- **the std door**: RULED the drafted set + sys.args() / sys.name() /
  sys.err() + math.idiv. growth is post-v1, under the semver window.

next stop: P1 (patch 53), the honesty warmup.

[session 42: P1 DELIVERED as patch 53 - the stamp, ok create, OK_ROOT,
the -Werror gate + the 530-warning sweep, the CLI test lane. gates:
e2e 510x7, capi 325x5, cli 10/10, delta blue flat. P2 is next.]

[session 42, later: P2 DELIVERED as patch 54 - ok_utf8 + the front door,
codepoint-first iteration/index, len = codepoints cached on the interned
header, the string-scanner rework + \u{} escapes, and the for-slot
call-window fix the conformance check exposed. gates: e2e 530x7, unit
54-check oktest_utf8, capi 325x5, delta blue flat. P3 next.]

[session 43: P3 part one DELIVERED as patch 55 - f"" interpolation +
m""/fm"" multiline. the shape: an f-string stays ONE token, and its
interpolation extents come from ONE byte scanner both the lexer and the
parser ride (ok_fstring_region_end) - braces nest, quoted strings and
line comments hide theirs, and a quote riding an f/fm prefix inside a
region takes the same scan recursively, so `f"${s("}")}"` and friends
work and the extents can never drift from the token's own boundaries.
the parser re-opens the token, chunk-splits the body, sub-parses each
${...} region with a region-anchored lexer (the closing brace is the
sub-parse's EOF wall, positions stay absolute), and desugars the whole
literal to string + to_string(expr) chains over the existing infix ADD -
OP_TOSTRING/OP_TOSTRING_LONG are exactly the as str lane in opcode form,
constant operands included (f"${1}" never materializes the 1). the
rulings all landed: `\${` the literal dollar-brace (f lanes only - plain
strings and m"" keep the fixed escape set and refuse `\$`), `$` without
`{` just a dollar, braces literal in bodies, NO format-spec language
(`:` after the expression is a located reserved error), mf" stays the
identifier `mf`, the unterminated errors name the opening line, and
newline-in-single-line still dies. 14 new fixtures under strings/
(incl. 9 error lanes; the m"" opening-line and the line-anchoring
rulings are pinned by substring on the reported line:col). gates: e2e
544x7 (530 + the 14), unit 5x3, capi 325x5, cli 10/10, delta blue
flat. P3 remainder - the str method toolkit + std.math + the
deprecation windows - rides as patch 56.]

[session 43, later: P3 COMPLETE as patch 56 - the str method toolkit
(12 methods on the class: trim split join contains starts_with
ends_with index_of upper lower repeat bytes codepoints; byte-exact
matching, codepoint indexes, ASCII-only case fold, null-for-absent),
std.math (20 members: the deterministic cmath surface + the RULED idiv,
finite in finite out, every domain violation and overflow a located
hard error), and the deprecation windows opened in oklang's README
(push/pop/remove -> class canonical, the print statement ->
std.io.print, %g -> std.io's shortest-exact; docs-only this wave, the
removals are v1.0.0's). gates: e2e 560x7 (544 + 16), capi 325x5, cli
10/10, delta blue flat at 1.92x/1.87x. P3's semantic calls (split's
required separator, join's strings-only membership, index_of's
null-for-absent, the ASCII-only folds) + the math-constants question
(pi, e need a registry lane for non-native members) ride as OPEN items
for Q in the patches entry. the wave is 3/7. P4 next: fs/env/clock
behind OK_CAP_HOST.]

[session 44: the two P3 loose ends TIED (patches 57 + 58, both
delivered). Q's round: the math constants approved ("worth it before
v1 if it is not very complex") - and it was not, because the registry
was value-blind all along: pi and e are hidden NUMBER globals harvested
by two table rows, no new machinery, no flat spelling, the module door
only (from_missing's pin moved to nan, permanent under the no-NaN
doctrine). the P3 semantic calls stand (Q agreed "for now"). the
escape-set question ("all ansi escapes... shouldnt we do it all?")
answered with the full set: \a \b \e \f \v \0 \xNN in the parser's
unescape switch, the scanner untouched (meaning-free by design), with
the two honest limits - \x stops at ascii (literals produce valid
utf-8 only, \u{} is the codepoint door) and no octal (\0 is exactly
nul, digits after it refused). \e is 0x1B: the ANSI color spellings
ride it. both patches stack clean on 54fd30f (patch 56's landing,
verified zero-drift this round). gates: e2e 560x7 then 564x7 (4 new
error lanes), unit + embed 5x3, capi 325x5, cli 10/10, delta blue flat
(1.92x/1.87x, then 1.93x/1.88x). the wave is 3/7 with P3's tail tied.
P4 next: fs/env/clock behind OK_CAP_HOST.]
