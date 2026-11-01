# oklang 0.4.0 (Eyrie) — the errors patch series, a reading guide

status: the complete walkthrough of patches 23–28, written to be read whole.
ERRORS.md is the design record (rulings, receipts, the why); this is the
code-adjacent story (the how, the machinery, the places to look twice).
read order for a review pass: §0, then §6 (defer — newest, biggest), then
whatever the eye wants. every claim here was verified against the tree the
patches produce; the gates at the end are the receipt.

---

## §0 — the shape of the phase

errors 0.4.0 is **two halves, one philosophy**: the user-facing model
(throw/catch, errors as values, handler frames) and the reporting layer
(okdiag, notes, source ownership). they ship together because they are the
same feature to the user — "errors" — and because every later phase
(classes, coros, modules) leans on the reporting half.

the phase lands as six patches, in strict order, on top of master
`0193e08` (post-spans):

| # | patch | one line | the ruling it carries |
|---|-------|----------|----------------------|
| 23 | diag-core | one pretty printer, three tiers | R1–R6: the receipts |
| 24 | runtime-locations-and-notes | the FAIL_PATH lane, located notes | R4, R10 |
| 25 | source-ownership | the vm source table, opt-in retention | E8, E6, R7–R9 |
| 26 | throw-catch | handler records + the unwinder | D7, E2, E3 |
| 27 | c-api | ok_last_error / ok_clear_error / ok_native_throw | E7 |
| 28 | defer | kill finally, embrace defer | E4, superseded |

patch 27 exists in a **v2**: the original 27 carried finalize + the C API;
Q killed finalize before applying anything ("why have 2 ways if one do
trick?!") and defer took its place as patch 28. the C API survived the
split untouched — it never depended on finalize.

numbers, for the impatient: e2e **342×4** (305 at master → +37), unit ×3,
embed ×3, fresh-apply of 23→28 on clean master builds pedantic-clean and
passes 342/342. the spans corpus disassembles **byte-identically** except
one intended fix (§6.5).

---

## §1 — patch 23: okdiag, the renderer

**the problem it kills**: every diagnostic in the language was broken in a
different way. columns were absolute file offsets (R1 — the lexer never
reset on '\n', `match()` never counted). the "source line" printed under an
error was the TOKEN TEXT (R2) — an immutable-mutation error rendered its
line as `=`. every parse error carried a phantom empty note (R3). runtime
errors from the dispatch loop had no location at all (R4). EOF errors
rendered nothing (R5). error tokens pointed their "source" pointer at the
message string (R6 — a landmine under any future print-the-line logic).

**the fix is one pair of files** (`okdiag.c/h`) and one discipline: the
diag boundary takes **spans, not tokens** — `{line, byte offset, length}`,
computed by pointer arithmetic at token creation (`token.start -
source_begin`), which kills R1 *by construction*: there is no accumulation
left to get wrong.

the output shape is gcc-parseable headers + a rustc-style snippet:

```
/tmp/t7.ok:2:1: error: attempting to mutate an immutable local variable.
    2 | x = 2;
      | ^
/tmp/t7.ok:1:5: note: declared here without 'mut'
    1 | let x = 1;
      |     ^~~~~
```

things worth knowing when reading the code:

- **notes get locations.** a note without a span renders as before; a note
  with one renders header + snippet. this is the whole "note: previously
  declared here" feature — it just needed binders to remember (patch 24's
  half of the bargain).
- **the caret underlines the full token length** (`^~~~~`), min one `^`.
  EOF spans point one past the last byte and say `(at end of file)`.
- **byte = char for now** (E5: Q's ruling — tabs are one column and
  render raw; the 0.8.0 utf8 pass touches exactly one function).
- panic mode stays: first error wins. no colors (the e2e runner matches
  predictable bytes — colors are 0.9.0 tooling).
- the RLE line table gained a debug assert (`sum(reps) == code.count`) —
  the wrapped-index NULL from R4 once was silent; now it's loud.

**where to look**: `okdiag_report()` — everything funnels through it;
`oklexer.c`'s token creation (the pointer arithmetic); the six new e2e
tests pinning the exact two-line snippet format as a contract.

---

## §2 — patch 24: runtime locations + the notes

**the R4 fix is the interesting one**, because the obvious version was
measured and rejected: syncing `frame->ip = ip` at the top of the dispatch
loop costs +10–18% on `numeric_for_sum` (the loop's hottest locals get
forced to memory). the shape that shipped is lua's savestate/Protect
model turned inside out:

- `runtime_error()` **only stashes** — kind + text into the error lane, no
  print, no reset, no allocation.
- the failing arm's `FAIL_PATH()` is the one place the live `ip` and the
  call stack still exist together — it owns the sync, the unwind attempt
  (patch 26's lane), and the report decision.

this pulled patch 26's error lane forward into 24 — which is why 26 reads
as "purely the model".

the rest of the patch:

- **binder receipts**: the scope map stores register indices; a parallel
  `declarations` array stores `{register_index, declared_token}` — the
  "previously declared here" / "declared here without 'mut'" notes read it
  at error time. locals AND upvalues (the upvalue note points into the
  declaring function's source — the draft only promised scope machinery).
- the R10 sweep: pluralization, trailing periods, "got:" colons, one
  "convertable", gcc-parseable trace headers.

**where to look**: the `FAIL_PATH` macro in `run_until`; `scope_declaration`
in okcompiler.h; the located-note e2e tests.

---

## §3 — patch 25: source ownership

the disqualifying embedded-language bug (R8): the vm borrowed ONE source
pointer. after `run_file` returned, `vm->source->code` dangled into freed
heap; the REPL dangled into a dead stack frame; a function compiled from
source A called during source B's run reported B's path with A's lines.
and once the renderer prints source EXCERPTS at runtime, a dangling `code`
pointer stops being cosmetic and becomes a segfault.

the v1 that shipped (E8 — **opt-in**, Q's catch: "future okc may not have
the source at all"):

- **ok owns the path, always** (interned copy; a chunk can name its source
  after the run that compiled it ended).
- **the code is the caller's for the run, ok's copy with retention on**
  (`ok_retain_source(ok*, bool)`, default OFF). the REPL opts in (its
  reused stack buffer demands it); the CLI borrows; future okc pays
  nothing.
- **chunks remember their source id** into a per-run table on the vm: an
  A-compiled leftover called during run B reports A's path with A's
  line:col, excerpt only while A's text is alive. "last-source-wins" is
  gone entirely — right coordinates, excerpts when possible, never wrong,
  never dangling.
- `ok_source.from` / `OK_FROM_FILE` / `OK_FROM_REPL`: **deleted** (E6 —
  write-only since birth, R9). the 0.9.0 REPL pass can reintroduce a kind
  WITH a consumer.

the full registry (keyed, deduped, module-era) stays in 0.7.0 where it
belongs — the roadmap keeps it.

**where to look**: `vm_sources` in okvm.h; `vm_begin_source` /
`vm_end_source`; the embed tests (`stack-buffer` reuse, run-A-then-B-then-
call-A-erroring: right path, no excerpt, no crash).

---

## §4 — patch 26: throw/catch, the model

D7's answer held against the post-spans code: **errors are plain values,
handlers are bytecode, and the VM is one C dispatch loop — so unwinding is
a loop, not a longjmp.** the pre-existing `error_pending` lane (proven
through nested native↔script hops by the embed tests) widened from
"string + bool" to "value".

the surface:

```ok
fu risky(d) {
  if d == "boom" { throw "bad input"; }   // any value can be an error
  return d;
}

try {
  print(risky("boom"));
} catch e {                               // E3: no parens anywhere —
  print(e);                               // `catch e { }`, or bare `catch { }`
}

throw;                                    // bare rethrow, inside a catch only
```

**every runtime error is catchable** — type errors, arity, out of bounds,
bad keys, div by zero, all of it, one lane. ONE exception: **OOM is not a
value and not catchable** (`runtime_oom`: the interpreter cannot be trusted
to build an error value when allocation failed — its own hard lane).

the mechanism:

- `call_frame` grows nothing; **handler records live on ONE vm-wide,
  frame-tagged stack**: `{frame, catch_ip, park, floor}`. scripts without
  try pay nothing — the stack stays empty, zero allocations.
- `try { B } catch e { H }` compiles to `TRY_ENTER` (push record; park =
  the binder's own register; floor = the try scope's checkpoint) / `B` /
  `TRY_EXIT` / jump; `catch_target:` lands the unwinder's park.
- **the park trick** (spans paying rent): the catch binder is an ordinary
  local. the unwinder writes the error value straight into its register —
  `OP_CATCH_BIND` was drafted, then killed: the record carries the park
  register and delivery needs no opcode.
- **the caught value** (E2, Q's field names): script `throw v` → the value
  whole. runtime-raised → `{:kind: "...", :what: "..."}`, materialized
  ONLY when a catch (or ok_last_error) consumes it. kinds: type / arity /
  index / key / value / conversion / native / internal — a closed set,
  consumer-driven (the oklangcpp error-code enum stays dead: nothing ever
  consumed those codes, and un-consumed taxonomies rot into lies).
- **the unwinder** walks records top-down; first live record above the C
  depth wins: pop the frames above it, close upvalues above the floor,
  restore the watermark to the handler frame's full window, park, jump.
  records below the depth belong to outer C levels — theirs to take.
- the trace is captured **lazily** — at throw time only where we stopped;
  the expensive walk+format happens only when the report prints.

two perf bugs found and fixed while building it, both worth an eye: the
unwinder's out-params made the loop's hottest locals address-taken
(+15% on numeric_for_sum — fixed with a by-value resume struct); an early
cut printed the report at every C level, so an error crossing a native
and getting caught still printed "uncaught" on the way through (fixed:
FAIL_PATH propagates silently, only the outermost lane decides).

**where to look**: `handler_record` in okvm.h; `attempt_unwind()` (before
patch 28 reworks it — read §6 after); the `{:kind:,:what:}` materializer
under its gc guards; the try battery in tests/e2e/tests/errors/.

---

## §5 — patch 27: the C boundary

C cannot run a catch block — bytecode handlers are invisible across
`ok_call`. so the contract is status-shaped, and it was half-built
already:

- `ok_call` returns false on error — unchanged shape, proven machinery.
- **new** `ok_value ok_last_error(ok*)`: the error from the most recent
  failed run/call — your thrown value as-is, or a fresh `{:kind:,:what:}`
  map for runtime-raised ones. unrooted by contract: root it (`ok_root`)
  or finish your allocations before fetching.
- `void ok_clear_error(ok*)`.
- `bool ok_native_throw(ok*, ok_value)`: the host's value crosses into
  script catch as itself — the sibling of `ok_native_error`, which stays
  as message sugar (it throws `{:kind: "native", :what: <message>}`).
- **E7, the one embedder-visible behavior change**: a failed ok_call
  prints NOTHING. the uncaught-to-top report stays ok_run's. embedders
  who want the print hold the value already.

**where to look**: the embed test's E7 section — including the part where
the test author (me) violated ok_last_error's rooting contract under
aggressive GC and the test now *demonstrates* the contract with ok_root
instead of avoiding it. that's the review-worthy paragraph.

---

## §6 — patch 28: defer (the new one — read this one hardest)

### 6.1 the ruling and the shape

Q's words: **"kill finally, embrace defer. why have 2 ways if one do
trick?!"** — plus a half-asleep sketch to validate: per-scope, zig style
("i take it back, zig style is superior — why function level where they
accumulate"); LIFO; "special local variable(s) with a special value (a
closure to call)"; runs "before killing any other local, and before
upvalues closing — one step of live code before the usual cleanup"; an
errdefer sibling under the name **deferr** ("name is creative gotta give
myself that 😭"); args evaluated at the defer site; and a suspicion that
OP_CLOSE_UPVALUES could be repurposed into an end-scope op.

the validation, honestly:

- **per-scope: yes, and it's the whole point.** finalize v1's documented
  gap was that break/continue/return skipped it. per-scope defers make
  every exit a scope exit — there is no path out of a scope the compiler
  doesn't own.
- **the repurposed CLOSE_UPVALUES: declined.** a merged end-scope op would
  have to CALL from inside an opcode handler with re-entrant frame state
  (a call mid-op that returns *past* the op's remaining work). plain
  per-exit emission — the existing CLOSE_UPVALUES conditional-emission
  pattern, extended — is strictly more boring, and boring was the brief.
- **the "special value wrapping a closure": yes, exactly one tag.** defer
  vs deferr never needs runtime distinguishing (the compiler knows which
  slots to RUN and which to KILL) — so ONE NaN-box tag covers both, and it
  spends the box's LAST free slot (§8 has the storage story).

### 6.2 the mechanism

`defer f(a, b)` compiles to, inside the current scope:

```
<hidden locals " d7c", " d7a0", " d7a1", " d7s" declared in the scope>
<callee evaluated into a temp, moved to " d7c">      ; the SNAPSHOT:
<arg evaluated into a temp, moved to " d7a0">        ; read NOW, not at exit
CLOSURE " d7s", k[thunk]                             ; fu() { d7c(d7a0, d7a1) }
DEFER_MARK " d7s"                                    ; the slot is a pending defer
```

- **the snapshot** is the "expression should definitely evaluate" ruling:
  callee and arguments are read at the defer site, into hidden locals
  (leading-space names, unique per statement via a parser serial).
  reassigning `a` afterwards changes nothing.
- **the thunk** is a parser-built AST — identifiers over the hidden names,
  one call, zero parameters — compiled by the **ordinary lambda path**.
  this is the load-bearing boringness: everything a call can do (default
  arguments, closures over outer mutables, multi-result callees in the
  discard lane, a defer inside a defer) is inherited for free. the
  thunk's identifiers resolve through the scope map onto the snapshot
  locals and capture them as upvalues — open, pointing at the registers,
  closed by the scope's ordinary CLOSE_UPVALUES *after* the defers run.
  Q's ordering, kept to the letter.
- **the slot + the tag**: OP_DEFER_MARK wraps the thunk in TAG_DEFER. a
  tagged value exists ONLY in its slot — never on the expression stack,
  never in user bindings, never even in the runtime's staging window.
  a null slot is a defer that never executed on this path; DEFER_RUN
  no-ops on it.
- **the normal exits are compile-time visible** (the "no one can skip it,
  not continue, break" ask): scope end, break, continue, return (all
  three shapes), and falling off the end each emit `DEFER_RUN` per
  pending defer — reverse declaration, LIFO — plus `LOAD_NULL` kills for
  deferr slots. the loop's own scope cleans at the break target, so
  break only pays emission for the scopes it actually crosses.
- **the unwind exits are the runtime's**: the unwinder scans register
  ranges for tagged slots — each dying frame's full window, innermost
  frame first, then the handler frame's region at/above the record's
  floor — running them descending. LIFO falls out of register order:
  later declarations hold higher registers, the slot last. thunks stage
  at the register-file top under a gc guard; the error lane parks in C
  while a thunk runs (§6.4, bug 3).
- **deferr** registers identically (same tag, same slot discipline) and
  differs in exactly one bit: never RUN-emitted. only the unwind scan
  executes it. a scope that exits normally — including an error caught
  INSIDE it — kills the slot without calling.

### 6.3 the semantics (the part to review hardest)

- **LIFO everywhere**: within a scope, reverse declaration; across
  scopes, innermost first; across frames, dying frames before the
  handler's region. defer + deferr interleave by declaration position.
- **cascade**: an error that passed through a defer invocation (raised in
  the thunk, or in anything the thunk called) stops the chain — the
  remaining defers in the dying region are nilled, not run. the failing
  defer's error **replaces** any in-flight one (finalize's E4 rule,
  inherited). the unwinder implements this by flagging defer-thunk
  frames and checking, before each scan, whether the error's path
  crossed one.
- **who can catch a defer's failure**: NOT the catch whose region was
  unwinding — its record is consumed before the scan runs, which is
  exactly Q's "can't be catched by the function where defer was made",
  holding precisely where he intuited it (the finally-replacement case).
  handlers further OUT — same frame or caller — can. the one deliberate
  deviation from the phrasing: a defer failing at a NORMAL scope exit
  inside a lexically-enclosing try IS catchable by it (the exit point is
  inside its region). special-casing that would cost a frame flag on the
  hot return path for a corner; declined, documented.
- **uncaught**: before the final reset, every frame's defers run — the
  sweep — innermost first. the report has already printed; the sweep's
  output lands after it. a defer failing during the sweep is dropped
  (nothing is left to consume it).
- **OOM**: the hard lane. no defers. you cannot trust the interpreter to
  run script when allocation failed.

### 6.4 the three bugs the gates caught

all aggressive-GC/ASan finds. this is what the gates are for.

1. **the main chunk's epilogue was dead code.** the compiler emitted the
   fall-off `RETURN` *before* ending the user top-level scope — so a
   top-level `defer` (and any top-level upvalue close) compiled to bytes
   after the return. fix: end the user scope first.
2. **the staging resurrection.** staging a thunk wrote its pointer at the
   register-file `top` — but `top` is the *deepest live frame's* window,
   which can sit BELOW an outer frame's compile-time window (a callee
   reuses the caller's temp region). the parked pointer got swept while
   uncovered, then re-covered — and marked — as a dangling reference by
   the outer frame's window. fix: staging writes NULL; the closure roots
   through the gc guard and then the pushed frame's own marking. a plain
   thunk value never exists outside its slot and its frame.
3. **the deferred-native spurious failure.** `call_native`'s post-flight
   check ("a native returning after an inner error must fail its
   OP_CALL") cannot tell an error raised DURING the native from one
   pending since BEFORE it — and the unwind scan always runs with an
   error in flight. every `defer len(x)`-shaped defer failed spuriously
   and turned catches into uncaughts. fix: the scan parks the
   propagation flag in C while the thunk runs (`has_error` /
   `error_value` stay set — the in-flight value must keep rooting).

### 6.5 what existing programs see

one bytecode-visible change, intended, verified: **top-level
CLOSE_UPVALUES now precedes the fall-off RETURN** (it was dead code
after it). the spans corpus is otherwise byte-identical — checked by
full disassembly diff.

### 6.6 v1 notes (deliberate)

- no spread in a defer call — `defer f(xs...)` is a clean compile error
  with a note pointing at the wrapper idiom (`defer run_all(xs)`).
- there is no braceless `if` in oklang, so a conditional defer always
  lives in the branch's own scope and runs at that scope's exit —
  immediate, and correct. the null-slot RUN path (a break crossing a
  scope before its defer statement) is tested.
- tree-sitter: `defer_stmt` / `deferr_stmt` added, `finalize` removed,
  corpus updated. no tree-sitter CLI in this sandbox — regenerate.

---

## §7 — the semantics reference

```ok
// scheduling: evaluated at the defer site, called at scope exit
{
  defer close(file);          // runs on EVERY exit of this scope
  deferr rollback(tx);        // runs only if an error unwinds OUT of it
  ...
}                             // normal end: defer runs, deferr dies

// every exit, no exceptions:
//   falling off the '}'        break        continue
//   return (bare / value / span)            fall-off-the-function
//   an unwinding error (this frame or a callee's)

// ordering: LIFO within a scope, innermost scope first, dying frames
// before the handler's region. defer and deferr interleave.

// errors:
//   a failing defer's error replaces the in-flight one
//   the chain cascades: subsequent defers in the dying region are skipped
//   the catch whose region was unwinding never sees it; outer handlers can
//   uncaught: all frames' defers run before the interpreter gives up
```

| question | answer |
|---|---|
| are args evaluated now or at exit? | **now** — reassignment changes nothing |
| what runs? | an ordinary zero-arity call of the snapshotted callee |
| defaults / closures / multi-result callees? | all work (thunk = lambda path) |
| `defer f` without a call? | compile error: `'defer' schedules a call, like: defer close(file).` |
| deferr after a caught-inside error? | killed, not run (the scope exited normally) |
| OOM | no defers run (the hard lane) |

---

## §8 — the NaN-box storage answer (Q's question)

"take a look at okvalue.h where the nanbox is — i dont think there is a
better way to do it, or is there? because the enum values are scattered
between 3 bits, so we can get more values."

**verdict: there isn't a better way, and the scattering is not a cost —
it's the budget.** walk the constraints:

- the payload must stay **48 bits**: pointers, and INT sign-extends
  across all of them. any tag bit stolen from the payload shrinks INT or
  breaks pointer round-tripping.
- bit 51 must stay set (quiet-NaN hygiene — tagged values shouldn't trip
  signaling-NaN traps in debug FP environments).
- bit 50 must stay set **as the hardware-NaN guard**: x86-64 arithmetic
  produces `0x7ff8...` (bit 50 clear) NaNs; with bit 50 required in the
  box, those fail `IS_VALUE_NUMBER`'s exact-pattern test and stay numbers.
  drop the guard and every hardware NaN becomes a candidate null.

that leaves exactly bit 63 and bits 49–48 — the scattered three bits the
enum uses. contiguous alternatives all give something up: low-bits
tagging needs aligned payloads (dead for doubles); raw-pointer tagging
("if bits 63–48 are zero it's a pointer") breaks the day LA57 machines
show up, and oklang embeds in hosts it doesn't control; QNAN = 0x7ff8
collides with hardware NaNs as above.

one correction the defer work made along the way: the header's "reserved,
not used yet" comments were half a lie — **TAG_RESERVED_1 was already
spent** (it carries the `as`-conversion type tags, TT_STR/TT_NUM/TT_BOOL —
see `perform_as` and the `" str"` intrinsics). the rename makes the truth
readable: the tags are now NULL / FALSE / TRUE / OBJECT / INT / USER /
AS / **DEFER** — the box is full, and the defer tag is the last one in.

the honest caveat that comes with every QNAN box: a *crafted* NaN (bit
pattern exactly `0x7ffc...`) aliases the tag space — a 2⁻⁵¹-ish collision
for random doubles, craftable by a determined host. every NaN-boxing
language ships this hole; the standard mitigation (canonicalizing NaN
payloads at the number boundary) trades arithmetic NaN-payload
preservation for tightness. noted for the float-attention era, not
changed now.

---

## §9 — what to review hardest

1. **§6.3, "who can catch a defer's failure"** — the one ruling that
   extends Q's words rather than quoting them. if the same-frame
   enclosing-try case should NOT catch, it costs a frame flag on the
   return path; i judged the corner not worth the hot-path store.
2. **the three bugs (§6.4)** — especially the staging resurrection: the
   invariant worth internalizing is "top is the deepest frame's window,
   not the max over frames". anything that parks a value at `top` and
   expects it to stay dead-or-alive consistently must not exist — which
   is why the staging writes NULL.
3. **the bytecode delta (§6.5)** — one intended reordering; the corpus
   diff is otherwise empty.
4. **patch 25's opt-in default** (borrow, not copy) — right for okc,
   costs the REPL one explicit line (it opts in).
5. **the kind set** (E2) — closed on purpose; `std.exception` (classes
   era) wraps the same two fields, so the names are load-bearing.

---

## §10 — applying and verifying

```
git checkout master && git pull          # 0193e08 or later
git am --3way 23-diag-core.patch         # or: git apply, in order
git apply 24-runtime-locations-and-notes.patch
git apply 25-source-ownership.patch
git apply 26-throw-catch.patch
git apply 27-c-api.patch
git apply 28-defer.patch
```

gates (the receipt this guide was written against):

- e2e: 342/342 on plain / aggressive-GC / asan / asan+aggressive
- unit: array, map, nan — ×3 variants
- embed: ×3 variants (includes the defer uncaught-sweep section)
- fresh-apply of the whole series on clean master: sequential apply
  clean, `-Wall -Wextra -Wpedantic` zero errors, 342/342
- spans corpus: disassembly byte-identical except §6.5's fix

tree-sitter: regenerate (`defer_stmt`, `deferr_stmt`, no `finalize`),
corpus expectations included in the patch's tree-sitter delta.

— z, session 25. Continuum for the spans tag whenever you're ready;
Eyrie when this lands.
