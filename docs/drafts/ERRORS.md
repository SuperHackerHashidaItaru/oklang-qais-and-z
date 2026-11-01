# ERRORS — 0.4.0 candidate, feature draft, rulings OPEN

status: DRAFT for Q. carries D7 (DECISIONS.md — lua-model values + bytecode
handler frames, NO setjmp/longjmp) forward against the code as it stands
after spans (master 0193e08), and adds what D7 didn't have to decide: the
diagnostics half (Q's ask this session: pull `error_at*` into this round —
"they are bad and been broken since i wrote them"), gcc-style notes with
secondary locations, the source-ownership fix (the vm-retains-one-source
bug Q flagged), the `OK_FROM_*` cleanup, and the C-boundary contract.
read with ROADMAP-v1.md §0.4.0. the try/catch-vs-pcall question the
roadmap parked "at the coro session" is answered ON PAPER here (§6) so the
coro session inherits a settled design instead of reopening one.

the shape of the phase: **two halves, one philosophy.** half one is the
user-facing model (throw/catch, errors as values, handler frames). half
two is the reporting layer (what you SEE when something fails). they ship
in the same minor because they are the same feature to the user — "errors"
— and because the reporting half is the one every future phase (classes,
coros, modules) will lean on. reporting first, model second, per §7.

## 1. what ships (surface)

```ok
fu risky(d) {
  if d == "boom" { throw "bad input"; }   // any value can be an error
  return d;
}

try {
  print(risky("boom"));
} catch (e) {
  print(e);                                // "bad input" — the thrown value
}

try {
  let arr = [1, 2, 3];
  print(arr[9]);                           // runtime error...
} catch (e) {
  print(e["message"]);                     // ...caught as a value (E2)
}
```

- `throw expr;` — evaluates the expression, the value IS the error. stops
  at the nearest handler.
- `try { } catch (e) { }` — the handler. `e` is an immutable binding
  scoped to the catch block (E3). one catch clause v1 — no typed catches
  until classes exist (D7's argument: don't front-load class decisions).
- `throw;` — bare rethrow, legal only inside a catch block: rethrows the
  in-flight value without touching it.
- `finalize { }` — semantics RULED here (E4), implementation may trail:
  runs on normal exit AND on unwind, before the catch / before the error
  continues outward. never swallows: an error raised inside a finalizing
  block replaces the in-flight one (same as every mainstream precedent).
  the token stays reserved until its patch lands — the parser's current
  "error handling is not implemented yet" message keeps covering it.
- **every runtime error is catchable**: type errors, arity, out of bounds,
  bad keys, div by zero — all funnel through the same lane as `throw`.
  ONE exception: **out of memory is not a value and not catchable** — you
  cannot trust the interpreter to build an error value when allocation
  failed. OOM stays a hard stop (documented lane).
- uncaught error reaching the top: the report prints (renderer of §4),
  `ok_run` returns `OK_RUNTIME_ERROR`, CLI exits 70 — today's contract,
  with a better report.
- what does NOT ship v1: typed/multiple catches (classes era), error
  codes enums (§8 — the oklangcpp idea, examined and declined), error
  recovery / multi-error compiles (0.9.0, unchanged), colored output
  (0.9.0 tooling), any `->` syntax (still reserved, still untouched).

## 2. the model — D7 restated against the code as it is

D7's answer holds: **errors are plain values, handlers are bytecode, and
the VM is one C dispatch loop — so unwinding is a loop, not a longjmp.**
the evidence, post-spans:

- `error_pending` / `native_has_error` / `native_error_message` on the
  execution state (okvm.c) IS the propagation seed D7 predicted — it
  already carries an error through nested native↔script hops (the embed
  `host_apply(boom)` test proves the exact path). v1 widens it from
  "string + bool" to "value" and the whole model inherits the proof.
- `runtime_error(vm, fmt, ...)` is the single funnel every VM error
  already flows through (82 call sites) — it already resets the call
  stack and watermark. the model change is surgically inside this one
  funnel: instead of "reset to zero", it becomes "unwind to the nearest
  handler, or zero if none".
- the call machinery already syncs `frame->ip` before every C re-entry
  (the four CALL handlers) — resumability after a C hop is a solved
  invariant, and unwinding reuses it.
- `TOKEN_TRY/CATCH/THROW/FINALIZE` are reserved and parse to a clean
  error today (okparser.c:892) — the surface costs no compat budget.

### the mechanism (handler records, per frame)

blocks don't get frames (functions do), so the handler lives in the frame:

```
call_frame grows:  handler records stack (small array, arena-ish)
record          =  { catch_ip, reg_top_at_entry, upvalue_floor }
```

`try { B } catch (e) { H }` compiles to:

```
  OP_TRY_ENTER  catch_target          ; push {catch_ip, reg_top, floor}
  B
  OP_TRY_EXIT                        ; pop the record
  OP_JUMP end
catch_target:
  OP_CATCH_BIND  slot                 ; error value -> e's register,
                                     ; reg_top restored, upvalues >= floor closed
  H
end:
```

- `OP_THROW slot`: value comes off the register; walk frames top-down;
  first frame with a live record wins — close upvalues above the floor,
  pop the frames, restore reg_top, deliver the value at the agreed slot,
  jump. no record anywhere: today's path (report + reset + status).
- the trace is captured **lazily**: at throw time we only remember where
  we stopped; the expensive walk+format happens only when the report
  actually prints (uncaught, or the host asks). D7 verbatim.
- `catch` binder is an ordinary local (a register) — no new binding
  machinery, the span-work window discipline applies unchanged.
- nested try / try-in-loop / throw-inside-catch all fall out of the
  record stack: entering catch pops the record, so a rethrow naturally
  finds only outer handlers. `throw;` re-emits the in-flight value.

### the caught value (E2 — the one real design choice)

- script `throw v` → the value IS the error. `catch (e)` binds `v`.
  strings, numbers, maps, arrays — anything. D7's "any value" kept whole.
- a **runtime-raised** error must synthesize a value. v1 proposal: a map
  (we have maps now) — `{:kind: "type", :message: "invalid operands for
  binary '+'."}` — `kind` from a small closed set (type / arity / index /
  key / value / conversion / internal). why a map and not a bare string:
  `catch` code needs to branch on what happened, and strings force
  everyone into prefix-matching. why not wait for classes: the map IS the
  transition plan — 0.5.0's error object wraps the same two fields, and
  `e["message"]` code carries over. the alternative (string-only v1) is
  cheaper but every catcher re-derives structure later; flagged for Q.

## 3. the diagnostics half — receipts first

every claim below is reproducible on master 0193e08 (build-release/ok).

**R1 — columns are absolute file offsets, everywhere.** the lexer never
resets `line_info.offset` on `'\n'` (oklexer.c:380) and `match()` doesn't
count at all (oklexer.c:49) — so "column" is "advance() calls since file
start + 1". receipt: `x = 2 + ;` (2 lines, 21 bytes) reports `3:22`.
this was suspected in session 19's stress test; confirmed.

**R2 — the "source line" printed is the token text.** `report_at_noted`
prints `%.*s` of `token.start/length` (okutils.c:100) — so an immutable
-mutation error renders its "line" as `=`. the caret code below it is
`#if 0`-ed out (okutils.c:101) because a column can't align to a line
that isn't the line. broken since written, exactly as advertised.

**R3 — every parser error carries a phantom empty note.** the parser's
`error_at` passes an EMPTY-but-non-NULL note view (okparser.c:197) while
the compiler's passes NULL (okcompiler.c:190) — the NULL check prints
`    | note: ` with nothing after it. cosmetic, but it's on every single
parse error.

**R4 — runtime errors from the dispatch loop have NO location.** receipt:
`let a = 1 + "x";` prints `invalid operands for binary '+'.` with no
`path:line:col` at all, while an arity error from the same file prints
one. root cause: `run_until` caches `ip` in a local (okvm.c:1511) and
syncs `frame->ip = ip` only in the four CALL handlers — so
`runtime_error`'s `frame->ip - 1` is the frame's entry pointer for every
error raised directly from the loop (BIN_OP, INDEX, ...), the wrapped
index misses the line table, `chunk_find_line_info` returns NULL. errors
raised inside `call()` are the only ones with a location, which is why
the bug looks random. outer frames in traces are always right (their ip
was synced at their own OP_CALL); only the top frame lies.

**R5 — EOF errors render nothing.** `at end (EOF).` with no line, no
hint of which end. we have the source; we can show the last line.

**R6 — error tokens fake a source pointer.** `create_error_token` points
`token.start` at the MESSAGE string (oklexer.c:35) — under any future
"print the line at token.start" logic, TOKEN_ERROR would print the
message as if it were source. line info on error tokens is fine; the
pointer is a landmine.

**R7 — ok.h lies about ownership.** `ok_run`'s comment says "takes
ownership of code and path strings" (ok.h:29) — the CLI `free`s the
buffer itself after the call, the REPL passes a REUSED STACK buffer, the
embed tests pass string literals. nobody takes ownership; the contract
is fiction.

**R8 — one source per VM, borrowed.** `vm_interpret` stores the borrowed
pointer with its own TODO saying so (okvm.c:115). consequences: after
`run_file` returns, `vm->source->code` dangles into freed heap; the REPL
dangles into a dead stack frame on return; functions compiled from
source A that are still callable after source B ran (globals hold them)
would report B's path with A's line numbers. for an embedded language
this is the disqualifying bug Q named. and note what R8 + the new
renderer interact into: once we print source EXCERPTS at runtime, a
dangling `code` pointer stops being cosmetic and becomes a segfault —
which is why ownership lands in THIS phase (§5) and not later.

**R9 — `ok_source.from` is write-only.** grep: set by cli (x2) and every
embedder; read by NOBODY. `OK_FROM_FILE` / `OK_FROM_REPL` don't "do"
anything — the names promise a distinction the code never cashes. (E6.)

**R10 — small message rot.** "expected 1 arguments, got: 2."
(pluralization), the trace prints `in <main>.` with a trailing period
on every frame, redefinition errors carry no note although the textbook
note ("previously declared here") is exactly this case — the scope map
stores only the register index (okcompiler.c:333), the declaration's
position is simply not retained anywhere.

the old oklangcpp "error model" (62b429f, last pre-rewrite commit) was
looked at as requested: `error { code enum, line, offset, message,
line_content }` + collect-into-vector + `show()` at the end, panic mode
inside. two ideas worth stealing — it SNAPPED THE LINE CONTENT into the
error at raise time, and its codes were greppable. one idea declined:
the code enum (§8). the rest of that code is, as established, dogshit,
and stays buried.

## 4. the renderer (okdiag — one pretty printer, three tiers)

new pair `okdiag.c/h`. parse errors, compile errors, and runtime reports
all render through it. the shape (gcc-parseable headers — every
diagnostic line starts with `path:line:col:` so editors and CI can jump
— plus a rustc-style snippet):

```
/tmp/t7.ok:2:1: error: attempting to mutate an immutable local variable.
    2 | x = 2;
      | ^
/tmp/t7.ok:1:5: note: declared here without 'mut'
    1 | let x = 1;
      |     ^~~~~
```

- **notes get locations.** today `report_at_noted` takes a plain string;
  the new note is `{ text, optional span }`. a note without a span
  renders as today (`note: did you forget to declare it 'mut'?`); a note
  with one renders the header + snippet, as above. this is the whole
  "note: previously declared here" feature Q asked for — it needs
  binders to remember where they were declared (§5, patch 24).
- **spans, not tokens, at the boundary.** the diag API takes
  `{ line, byte offset of start, length }`. computed by pointer
  arithmetic at token creation — `token.start - source_begin` — which
  kills R1 by construction: no accumulation to get wrong, `match()`
  included. `ok_line_info` becomes `{ line, start_offset }` (the name
  `offset` keeps, the semantics change: file-absolute byte offset of the
  token start; columns are DERIVED, never stored).
- **column derivation is one function** — `column_of(source, offset)`:
  scan back to the previous `'\n'`, subtract. **byte = char for now**
  (Q's ruling; revisit at the 0.8.0 utf8 pass, which now touches exactly
  one function). tabs count as one column and render raw — the caret
  aligns to the raw text, which is the only thing that stays true on
  every terminal. documented.
- the caret underlines the token's full length (`^~~~~`), min one `^`.
  EOF spans point one past the last byte and append `(at end of file)`.
- **runtime reports use the same renderer**: location header, snippet +
  caret when the source text is alive AND the chunk's source is the one
  being excerpted (§5 gives chunks a source identity), else header-only
  (correct path + true line:col — degraded, never wrong). the stack
  trace keeps its shape (`in <main>`, most-recent-first) but each frame
  line gains a location header, and the top frame becomes CORRECT via
  the R4 fix (sync `frame->ip = ip` on the loop's error exits — the loop
  owns the live `ip` exactly where `interpret_result_error()` returns).
- panic mode stays v1: first error wins (`REPORT_STATUS_SKIPPED`).
  parser recovery / multiple errors per compile remains 0.9.0's. (the
  parser's `sync_state` already leaks two errors on some inputs —
  observed; formalizing that behavior is deliberately left to 0.9.0.)
- no colors v1 — the e2e runner matches substrings; predictable bytes
  are the feature. TTY detection is 0.9.0 tooling.
- the RLE line table gets a debug assert (`sum(reps) == code.count`) —
  R4's wrapped index silently returned NULL once; an invariant guard
  turns the next such bug into a loud one.

test impact: `// expect error: <substring>` matches on stderr substrings
and exit codes — unchanged by design. the caret/snippet lines get NEW
tests asserting exact two-line snippets (the format is now a contract we
test, not an accident we tolerate).

## 5. source ownership (the embedded-language bug, fixed minimally)

the full fix (many SIMULTANEOUS live sources) is module-era machinery —
a registry keyed by path, chunks referencing source ids. that design
stays in 0.7.0 where it belongs (ROADMAP-v1 updated). what 0.4.0 ships
is the honest v1 that makes the dangling impossible and the reports
never-wrong:

- **ok owns its copy.** `ok_run` duplicates `code` and `path` into the
  ok allocator; the vm points at the owned copy; `ok_free` releases it.
  the CLI stops freeing (ok's now), the REPL's stack buffer becomes
  safe-by-copy, embedders stop thinking about lifetimes. cost: one copy
  per run — the CLI's file buffer becomes this copy (net zero), the REPL
  line is ~1KB, embedders pass literals they were never going to mutate.
  the ok.h comment becomes TRUE: "ok_run copies both strings; your
  buffers are yours."
- **chunks remember their source.** `object_function`'s chunk grows a
  source identity (the path pointer of the owned copy + a code pointer
  for identity comparison). a runtime error in an A-compiled function
  called after B ran reports A's PATH with A's line numbers — right
  coordinates, no excerpt (excerpt only on identity match with the vm's
  current source). last-source-wins is now a DOCUMENTED limitation
  instead of a silent lie, and the 0.7.0 registry replaces it wholesale.
- **`ok_source.from` / `OK_FROM_FILE` / `OK_FROM_REPL`: deleted** (E6).
  write-only since birth (R9). the distinction they gesture at becomes
  real when something consumes it — the 0.9.0 REPL pass (input numbering,
  `repl:` headers) can reintroduce a kind WITH a consumer, and the
  embedder-facing constructor at that point should take
  `(code, path, kind)` explicitly anyway. pre-1.0, deleting a field
  nobody reads is the cheap honesty.

## 6. the C boundary + the coro paper (roadmap's "settled here")

**C boundary (the pcall shape lives HERE, where it belongs):** C cannot
run a catch block — bytecode handlers are invisible across `ok_call`.
so the C contract is status-shaped, and it is already half-built:

- `ok_call` returns false on error, today, with the machinery proven
  through nested natives. it keeps that shape.
- new: `ok_value ok_last_error(ok*)` — the error value from the most
  recent failed run/call on this state (the caught-value of §2: your
  thrown value, or the synthesized map). `void ok_clear_error(ok*)`.
- new: `bool ok_native_throw(ok*, ok_value)` — the value-throwing
  sibling of `ok_native_error` (which stays, and keeps its string
  convenience: it becomes sugar for throwing a message map).
- **host-caught errors stop auto-printing** (the uncaught-to-top report
  stays). today every ok_call failure prints to stderr; with a real
  out-channel that's noise. embedders who want the print still have the
  value. this is the one embedder-visible behavior change in the phase —
  flagged hard in the patch README (E7).

**coro paper (D7 verbatim, restated so the 0.6.0 session inherits it
closed):** handler records live in frames; frames live in a call stack;
each coro OWNS its call stack — so unwinding is coro-local by
construction. a coro that errors with no internal handler is marked
dead-with-error holding the value; `resume` returns a status + the error
value; rethrow across the boundary is the resumer's explicit act. no
cross-coro surprise is even expressible in this design. the one open
edge the coro session still owns: yield-inside-catch/finalize ordering —
out of scope until there is a yield.

## 7. implementation plan (patches, the usual rhythm)

gates per patch: e2e 306×4 (grows to ~330×4 by the end), unit ×3, embed
×3, fresh-apply verification. bytecode discipline: patches 23–25 change
NO generated code words (the source_info tables' offsets change
semantics; the disassembly diff normalizes the `; line` gutter), and
26–27 only add opcodes for NEW syntax — the spans corpus must disassemble
identically throughout.

- **patch 23 — diag core.** okdiag.c renderer; span-from-pointers
  (`ok_line_info` resemantics); lexer true-offset fix (R1) + error-token
  source-position fix (R6); kill the phantom note (R3); EOF rendering
  (R5); RLE debug assert; e2e snippet-format tests for all three tiers.
  output format changes land here, once.
- **patch 24 — runtime locations + notes.** `frame->ip` sync on error
  exits (R4); binder spans in the scope machinery (register index →
  index + span) feeding "previously declared here" / "declared here
  without 'mut'" notes; message sweep (R10: pluralization, trailing
  periods, a few author-token choices); trace frames gain location
  headers.
- **patch 25 — source ownership.** ok-owned copy (R7/R8); chunk source
  identity + never-wrong paths; CLI/REPL/embedder updates; `from` field
  deletion (R9/E6); embed tests: stack-buffer source, run-A-then-B-then
  -call-A erroring (right path, no excerpt, no crash); ok.h comments
  tell the truth.
- **patch 26 — throw/catch.** handler records + OP_TRY_ENTER /
  OP_TRY_EXIT / OP_CATCH_BIND / OP_THROW; catch binder; bare rethrow;
  runtime_error funnel gains the unwind lane (unwind-to-handler vs
  reset-to-zero); caught-value synthesis per E2; e2e: throw-any-value,
  nested try, try-in-loop, throw across native ok_call boundary,
  catch-then-recover-then-continue, uncaught report; embed section.
- **patch 27 — finalize + C API.** finalize blocks per E4 ruling;
  `ok_last_error` / `ok_clear_error` / `ok_native_throw`; host-caught
  auto-print removal per E7; embed section. (if E4 rules finalize out of
  0.4.0, this patch shrinks to the C API and the token stays reserved.)

## 8. what we are NOT doing (and the oklangcpp verdict)

- **no error-code enums.** the old model's `code::local_redefinition`
  etc. died for the right reason: nothing ever consumed the codes, and
  un-consumed taxonomies rot into lies about the language. the modern
  equivalents that ARE worth having — a `kind` FIELD on the synthesized
  runtime-error map (§2) — is consumer-driven (catch code branches on
  it). if tooling ever wants stable machine-readable ids, that's the
  LSP era adding them with a consumer in hand.
- no recovery/multi-error (0.9.0), no typed catches (0.5.0+), no colors,
  no JSON diagnostics, no `finally` spelling change (the reserved token
  is `finalize` and stays `finalize`).
- utf8: byte=char everywhere in diag, ONE function to revisit (§4).

## 9. rulings — OPEN (for Q)

- **E1 surface**: try/catch/finalize as §1 (keywords already reserved).
  confirm, or amend the shape.
- **E2 caught value for runtime-raised errors**: map `{:kind:, :message:}`
  (Z's lean — branchable, classes-ready) vs bare string v1 (cheaper,
  less committal).
- **E3 catch binding**: `catch (e)`, immutable, block-scoped; bare
  `catch { }` (no binder) also legal? Z's lean: yes, both.
- **E4 finalize**: ship in 0.4.0 (patch 27) vs keep reserved with
  semantics ruled (runs on normal + unwind, before catch; an error
  raised during finalize replaces the in-flight one). Z's lean: rule
  now, ship if the patch budget holds.
- **E5 columns**: byte-based v1, tab = 1 column, caret aligns to raw
  text; codepoint-based at 0.8.0 in exactly one function. confirm.
- **E6 `ok_source.from`**: DELETE the field + enum (Z's lean — it is
  write-only) vs rename to a meaningful kind with a consumer.
- **E7 C boundary**: `ok_last_error` + `ok_native_throw` + host-caught
  errors stop auto-printing (uncaught still prints). confirm the API
  shape and the behavior change.
- **E8 multi-source split**: own-the-copy + chunk identity now, full
  source registry at 0.7.0 modules (ROADMAP-v1 updated accordingly).
  confirm the split.

## 10. what spans/maps already bought us (reuse inventory)

- maps: the caught-value map (E2), and the module-era source registry
  (0.7.0) is another interned-path table — the maps core's second
  compiler customer.
- spans: the catch binder is an ordinary local — the register-window
  discipline from the destructuring work applies to delivering the error
  value with zero new binding machinery.
- the native-callback fix (pre-0.1.0): error_pending IS the propagation
  proof — this phase generalizes a proven lane instead of building one.
- the debug/disassembler infrastructure: the RLE assert and the
  instruction-level line printing it already does become the test
  fixtures for patch 23/24.
