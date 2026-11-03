# COROS — the coroutine lane (LANDED: patch 38, R20-R24 closed)

status: IMPLEMENTED on `opt-lightweight` (fed4158, patch 38), one session
after this draft. the rulings came back: R20 class + yield keyword (the
class is `coro`, lowercase, per R24's snake_case lean - always loaded for
now, future home `std.coro`), R21 resume-of-dead is null, R22 dead-by-error
defers run, R23 yield is a keyword, R24 ships now as a builtin. no
scheduler (standing veto; casio/asio is a later, separate conversation).

the draft below is kept as the design record; the deltas that landed:

- **construction is `coro(fu)`** - the overridden static `operator ()`
  (patch 34's door), not `Coro.of`. `c.resume([v])`, `c.status()`. the
  first resume's argument is the body's own argument; a suspended resume's
  argument lands in the parked yield expression (so `return yield;` parks
  null-out and the NEXT resume's argument is the return value).
- **the yield barrier (new ruling, mine):** a yield under a C-driven region
  - operator dispatch, construction, the defer UNWIND scan, a native
  ok_call hop - is a runtime error, lua's C-call-boundary discipline. the
  C frame holding the result bookkeeping cannot survive a suspension, so
  OP_YIELD refuses: "cannot yield from inside a constructor / an operator
  method / a defer", "cannot yield across a native call". this is why
  suspension never crosses C at all: RUNTIME_SUSPENDED is only ever
  produced by the resume driver's own run_until, and nothing between the
  yield and it holds state.
- **the surprise that works:** the NORMAL scope-exit defer lane
  (OP_DEFER_RUN) stages its thunks inside the dispatch loop - no C frame -
  so a defer containing a yield parks mid-cleanup and resumes honestly.
  only the error/unwind scan refuses. generators-in-defers are real.
- **errors transplant, not just propagate:** the death sweep runs the
  body's defers on the coro's own state (R22, exactly the uncaught-at-
  top-level discipline), then the whole error lane moves to the resumer's
  state - the resume call site is where a catch takes it, and an uncaught
  one reports once at the resumer's outermost level (the coro's frames are
  gone from the trace; the resume site is the recorded location).
- **upvalues needed real work the draft missed:** an open upvalue's location
  indexes its OWNER's register array, never the running state's. cells
  captured in the vm's own state resolve through `vm->main_state`; cells
  captured in a coro carry the coro as a TRACED owner value on the upvalue
  itself, so a closure that escaped a parked coro pins the coro (and its
  register file) alive until the cell closes - the sweep cannot dangle it.
  close clears the pin. steady-state read/write loops measure flat; the
  capture path pays one word (the churn micro ~10%).
- **the gc roots everything parked:** mark_vm walks the current state AND
  the `resume_parked` chain (resumers suspended under a resume) AND each
  reachable coro's owned state (the OBJ_CORO trace). aggressive-gc gate
  runs the whole suite parked-and-churning.

gates: e2e 423x423 (was 406) on threaded, pure switch, asan, and
aggressive-gc. 17 tests in tests/e2e/tests/coros/ including the eight
error lanes. two pre-existing bugs fixed en route: glob classes
published null (patch 37, Q's report), and parse_expression's infix loop
handed a NULL left to the next infix after any parse failure (a call node
with a null callable - the teardown segfaulted the cli).

---

the original draft, as written against the classes-complete tree:

status: DRAFT for Q, written against the classes-complete tree (your
`classes-staging`, a1411d1) plus the lightweight-perf pass that just landed
(patch 36: interning, poly-MIC, fused conditions, threaded dispatch). the
verdict i'm asking for is scope-and-surface only - the mechanics are
already 80% paid for by machinery that exists, which is the whole reason
this is "super easy but cool" instead of a project.

the one-line pitch: **a coroutine is a `run_until` level that parks instead
of returning.** everything else — the register window, the call stack, the
frame ips, the resume — is machinery the VM already owns and already
re-enters through (operator dispatch, defer, catch). the coro feature is
mostly a new object type that OWNS an `execution_state`, one opcode, and a
handful of natives.

## 0. what already exists and does the work

four receipts from the current tree, because they are the entire cost
argument:

- **`run_until(vm, depth)` re-enters and resumes.** the dispatch loop runs
  until the call stack drops to `depth` — and the catch lane already
  RESUMES a frame mid-body (`FAIL_PATH` parks the error value in a
  register, rewinds `ip`, `goto next_instruction`). suspension is the same
  move with a different reason code.
- **`dispatch_operator_method` (patch 32) is a nested `run_until`** with
  its own depth, its own staging window, and a result hand-off. the shape
  a `resume()` call takes is this function with the direction reversed.
- **`execution_state` is already a self-contained VM context**: registers
  (array + top), call_stack, handler stack, error lane. it is `malloc`'d
  once per `ok` today. a coroutine is: this struct, plus a marker of how
  far it has run.
- **the GC already traces a live execution state** (`mark_vm` walks
  `current_execution_state`). a parked coro is one more root of the same
  shape — the tracer is a copy-paste of four loops.

## 1. semantics (the part that needs rulings, not code)

```javascript
// the shape i recommend — a Coro is an object you drive explicitly.
// nothing about plain functions changes.
let producer = Coro.of(fu(n) {
  let mut i = 0;
  while i < n {
    yield i;              // hands a value OUT, parks the coro
    i = i + 1;
  }
  "done"                  // the coro's final value
});

let c = producer(3);      // creating: makes the coro, does NOT run it
print c.resume();         // 0    (runs to the first yield)
print c.resume();         // 1
print c.resume();         // 2
print c.resume();         // done (the body finished; coro is dead now)
print c.status();         // "dead"
```

- **`yield` is a statement AND an expression.** `let x = yield f();` parks
  the coro, and whatever the NEXT `resume(v)` passes lands in `x`. two-way
  channel, lua-shaped. (`yield;` is `yield null;`.)
- **resume from outside only.** a coro resumes from the code that holds
  the coro object, never spontaneously. `resume` on a running coro
  (re-entrancy, incl. the same coro from a native) is a runtime error;
  `resume` on a dead one returns null (Q: or error? — R21).
- **errors cross the boundary like returns.** a throw inside a suspended
  coro propagates to the `resume` that woke it (the coro dies, the error
  is the resumer's to catch). the machinery: FAIL_PATH already unwinds to
  a chosen `run_until` depth; the coro's depth is its own.
- **the body runs in its own call tree.** calls made inside the coro push
  frames on THE CORO's call stack, not the resumer's; a `yield` from a
  function called by the coro body still suspends the whole coro (this is
  what makes it a coroutine and not a generator loop) — the unwind to the
  coro's `run_until` depth is exactly the throw-unwind machinery pointed
  at a different depth.
- **construction, defer, try all work inside** — they already run inside
  nested `run_until`s (that's how operator dispatch runs `init`).
  pending defers at suspend time: PARKED, not run (they fire at the coro's
  death, whenever that is). R22 asks whether a dead-by-error coro runs
  its defers on the way out (my lean: yes, the cascade lane already does
  exactly this for failing frames).

## 2. the surface — three candidates, one recommendation

**A. `Coro.of(fu)` + `resume`/`status` (RECOMMENDED).** one builtin class
(rides the natives-as-classes door from patch 34 — `Coro` is a frozen
class value with native methods), `yield` is syntax. zero new stdlib
beyond the one type; composes with classes (a method can be a coro body:
`obj.run()` parks mid-method — it's just a closure with a receiver).

**B. wren-fiber style: `Fiber.new`/`call`/`transfer`.** more surface
(transfer = asymmetric-to-asymmetric handoff), more ruling surface, and
`call`-returning-the-yield-value vs `transfer`-not is a distinction we
don't need yet.

**C. lua-style free functions (`coro.wrap(f)`).** a library module, not a
class — but we have no module system until 0.6.0, and a global `Coro`
class is the same thing with the shape the language already uses.

`yield` spelling alternatives on the table: bare `yield` keyword (my
recommendation, above) vs `Coro.yield(v)` as a native that reads "the
current coro" off the execution state (no keyword at all — zero parser
surface, but `yield` stops being reservable and the call shape is
noisier). R23.

## 3. mechanics — the honest codegen sketch

**the object.** `object_coro { object; object_closure* body; value argv..;
execution_state* state; coro_status status; }` — OBJ_CORO rides
TAG_OBJECT. the state is allocated at `Coro.of`/first-resume (lazy: zero
cost for created-but-never-run coros).

**the execution split.** today `vm->current_execution_state` is THE one.
the change: resume SWAPS it (`saved = vm->current; vm->current = coro->state`),
runs `run_until(coro->depth)`, swaps back. the vm's own state stays
untouched in between — this is the same discipline `vm_call` already has
with `native_call_depth`, one level up. every VM entry point that caches
`exe` re-reads it after any call (the REFRESH_AFTER_NESTED discipline —
patch 32 taught every dispatch case this dance already), so a resume
under a native callback is sound by construction.

**`OP_YIELD` (1 word, A = the value register; a second word when the
resume-value register differs).** the handler: stores the resume-target
register index in the coro's saved frame data, parks the top frame's
`ip` (the same sync every error path does), unwinds the coro's frames to
its `run_until` depth WITHOUT running defers, and returns a new
`RUNTIME_SUSPENDED` interpret_result. resume: re-push nothing — the
frames are still there — stage the resume-arg into the parked
`yield`-expression's register, swap states, `run_until` again. the loop's
`next_instruction:` resume path is the template; `run_until` needs one
new exit condition, not a rewrite.

**the GC.** a parked coro's register window and call stack are its
`execution_state` — `mark_vm` grows a sibling `mark_coro` walk (four
loops, the same ones mark_vm runs: registers, frames' closures, open
upvalues — upvalues make coros compose with closures for free — and the
handler stack). the coro object roots its state; a dead coro frees it at
sweep like any object.

**what it does NOT do to the hot path.** one new opcode in the dispatch
table (NOP-cost for everyone else), one branch in `vm_call`-adjacent
plumbing, zero changes to the register file's stack discipline (the
coro's window is simply not the vm's window). this is why i'm confident
calling it cheap: the expensive parts (re-entrancy discipline, frame
parking, watermark restore) were all paid for by errors/defer/classes.

## 4. costs, honestly

- memory: a parked coro pins `registers * 8` bytes + `frames *
  sizeof(call_frame)` + the upvalue list. for a typical 24-register /
  3-deep coro that's well under 1kb. generators-as-coros at 100k parked
  would be ~100mb — a real number to know, not a blocker (wren fibers
  pin the same shape; lua grows a coroutine stack the same way).
- code: ~350-450 lines total (object + swap/yield/resume + mark + tests).
  the draft exists because the mechanics are receipts, not designs.
- the runtime_error surface: "resume of a running coroutine." etc. —
  three new messages.

## 5. test plan (the e2e shapes)

- basic out/in channel: yield as statement + expression, resume arg lands
- yield from nested calls inside the coro (the call tree is the coro's)
- coro calling a native that resumes another coro (state swap under
  native_call_depth > 0)
- error inside a suspended coro propagates to resumer; coro is dead after
- defers at suspend (parked) and at death (run), incl. error-death
- classes: a method as a coro body, construction inside a coro, yield
  inside init (the nested run_until construction lane)
- gc: aggressive-gc collect while parked (the mark_coro walk is the
  receipt this test exists to collect)
- the poly-mic / threaded-dispatch interaction: yield under both dispatch
  modes (the OP_YIELD handler is CASE()-shaped like every other)

## 6. the rulings, closed

- **R20**: A - class + yield keyword. **RULED: yes**, the class is `coro`
  (lowercase), hidden in `std.coro` when modules land, always loaded now.
- **R21**: resume of a dead coro. **RULED: null** (the iterator-exhausted
  shape) - including dead-by-error.
- **R22**: do a dead-by-error coro's pending defers run? **RULED: yes** -
  the death sweep runs them on the coro's own state before the lane
  transplants.
- **R23**: yield keyword vs native. **RULED: keyword.**
- **R24**: ship now or with 0.6.0's modules? **RULED: now**, lowercase
  `coro`, the eighth builtin class.
- standing veto upheld: no symmetric transfer, no coroutine priorities, no
  scheduler - a coro is driven by the holder's code, full stop. (the
  wren/lua schedulers are stdlib, and stdlib is 0.6.0+'s conversation;
  casio/asio is its own later project.)
- added by implementation: the C-boundary ruling (yield under C-driven
  regions errors), and upvalue ownership (open cells resolve through their
  owner's array, escaped closures pin their coro).
