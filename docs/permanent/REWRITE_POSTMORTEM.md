# the register VM rewrite — full account of what happened

this is the promised "full readme of what happened". scope: 22-25 oct 2026,
five working sessions, roughly forty bugs, from "the new VM has never executed
a program" to "the shipped game runs end-to-end on it, natively and on the
web, clean under ASan + aggressive GC". the gc/asan deep-dive already exists
(download/PORT_README.md) — this doc is the whole arc, written for future-us
when the next rewrite/port happens and someone asks "how bad was it, really".

## the shape of the thing that was built

oklang's register VM, as landed: one growable register file; frames live in
that file as windows (callee R0 = closure slot, params R1+; call windows
staged at the caller's temp frontier; caller cleanup restores top). two-word
instructions with a corrected wide layout op(7) p(1) a'(12) b'(12). upvalues
indexed ABSOLUTELY into the register file. globals resolved at compile time
to indices into an array-based store. values are NaN-boxed with tag bits at
48-49 + 63 — which matters later. jump offsets relative to the word after the
instruction; jump polarity `p ^ is_falsy`.

the old stack VM still exists and still runs the old game — that fact became
the most valuable test oracle of the whole project.

## phase 1 — first light (109/109 e2e)

the first sessions took the VM from zero to the full suite green. the bug list
from that phase reads like a taxonomy of porting mistakes, and every one of
them was a small invariant violation, not a design flaw:

- **instruction decode**: READ_WORD read through `frame->ip` while jumps used
  a local `ip` — every jump/call was broken. JOIN_WORD had the two halves in
  the wrong order, so every 2-word instruction decoded garbage.
- **missing breaks**: OP_CALL fell through to the next handler. twice.
- **frame plumbing**: return paths indexed the register struct as an array
  (memory corruption); call() never grew the register file nor set top, so
  callee frames ran out of bounds; fresh slots contained garbage doubles.
- **jump polarity**: `!p & is_falsy` meant p=1 never jumped — `||` silently
  didn't exist.
- **compiler slot metadata**: resolve_local double-indirected through a slot
  value (wrong metadata for ~every multi-local scope — the root of the
  "immutable local"/"undefined variable" cascades); rollback_temp desynced
  counts; statement-boundary temp resets truncated metadata below locals.
- **upvalue indexing**: semantic index vs array position confusion in three
  places — store_upvalue read OOB and emitted the wrong operand.
- **globals store**: descriptors never initialized (wild pointer SEGV) and an
  off-by-one that made STORE and LOAD hit different slots.
- **the link bomb**: chunk_write_word2 was declared, used, and never defined.
- **the assertion that fired on success**: `CRASH(!loop_stack_pop)` aborted
  every `while` at compile end, because CRASH fires when the arg is FALSE.

the phase-1 lesson: **a VM port is thirty small invariant bugs, not one big
bug.** the fix that actually mattered wasn't any single patch — it was making
okchunk.h's contract comments the single source of truth (jump base, polarity,
word order, operand semantics) and auditing against them. that habit is why
later phases went faster.

## phase 2 — the GC/ASan hunt (105/109, then 11/98, then all green)

full detail in PORT_README.md. the short version, because the shape of it
repeats everywhere:

- **aggressive GC (collect on every allocation) crashed 4 tests** with
  different root causes than the plain build. the killer: non-strict tag
  checks. NaN-box tag bits at 48-49 live inside double payload space, so
  `19.0` and `39.0` fake-decode as OBJECT — only doubles whose 4th-from-top
  nibble is 3/7/b/f — which is why only the `_long` tests (257 distinct
  constants) died and everything else passed. IS_VALUE_STRICTLY_* everywhere.
  the standing sharp edge: `IS_VALUE_INT(-1.0)` is true under non-strict
  checks — the STRICTLY macros should become THE macros someday.
- **a closure written AT `top`, rooted only in `[0, top)`** — the register
  equivalent of pushing without incrementing. one-line fix (`top += 1` after
  the write), classic category.
- **ASan "exploded" with 98 failures — zero corruption, all leaks**: the
  generated `*_pop()` never ran element deinits, so compiling ANY program
  leaked every scope, every loop context, every finished function context.
  the runner piped 98 full leak reports and looked like a catastrophe.
- **the embedding test was fake-OK twice over**: release builds compiled the
  asserts out (a no-op `assert(result == OK)` printed "OK" regardless), and
  under the real bugs, call_native read `call_stack.data[-1]` via an empty
  stack and wrote results to the wrong slot.

## phase 3 — "invalid call: expected 8 arguments, got: 0" (the game boots, then doesn't)

dorg3 ran its first frames on the new VM, called init — and hit an arity error
that made no sense. the diagnosis path is worth remembering because it's the
method that cracked every later bug:

1. the error had no native-call marker and an empty call stack -> the ok_call
   C-API path, not bytecode.
2. built a no-raylib harness (repro.c) reproducing the binder faithfully (86
   natives, 23 consts) — plain and ASan builds PASSED, the aggressive build
   reproduced byte-for-byte. (aggressive GC is a bug-finding instrument, not
   CI hygiene.)
3. OK_LOG_GC showed the same address swept twice, then reused by a different
   closure; only `<main>` was ever marked (983 times); init was NEVER marked.
4. measured: main's frame declared 103 registers; only `data[0..1)` was rooted
   during execution.

root cause: **call_native and cleanup_frame collapsed `top`** (the GC root
watermark) down to the callee slot / frame base on every native call and
return. after any top-level native call (load_font etc.), every fu-closure
local above the window was unrooted for the rest of main's execution. the
aggressive build swept them; freed closure memory got reused; init's global
slot aliased draw's 8-arity closure; ok_call(init) hit draw's arity. the fix
restores `top` to the CALLER's frame_top — plus: all perform_* error messages
had been commented out (silent RUNTIME_ERRORs), and runtime_error didn't
newline-terminate (the mystery `%` was terminal glue). this round gave the VM
its error-reporting spine: named callee, named operator, file:line:col, stack
trace.

## phase 4 — "expected argument of type 'music'" (the one-byte-off bug)

the game reached the menu and moved the dorg; then music_menu failed a type
check on a value that should have been a tagged music handle. repro2.c (the
harness, now binder-faithful with magic-tagged native values) reproduced it
byte-for-byte: `stop_music(g_current_music)` saw raw `0x0` — a double zero —
instead of tagged null, so `g_current_music != null` was TRUE for an unset
global.

opt-in OK_TRACE_GLOBALS instrumentation (LOAD/STORE_GLOBAL + LOAD_NULL traced
to stderr) found it in a single run: **OP_LOAD_NULL's interpreter loop treated
operand B as an exclusive END index while the chunk contract and the compiler
both say COUNT**. `for (i = A; i < B; ++i)` instead of `for (i = A; i < A + B;
++i)`. it only matched when A==0. every null materialized into r[k], k != 0
wrote NOTHING — stale register bits propagated. thirty broken LOAD_NULL sites
per game run, most passing BY LUCK because both sides of a null-vs-null check
held the same stale bits. one-line fix + a regression test that fails
pre-fix on all three build variants.

## phase 5 — "value is not callable" + the segfault AFTER the error (four bugs, one round)

first fork spawned, forks_update ran array_foreach(forks_positions_x, fu...) —
the project's first native->VM callback — and the VM reported the closure
uncallable, then segfaulted printing the error. repro3.c reproduced everything
byte-for-byte, including a full input-driving soak (click PLAY, WASD-dodge,
die, retry). four root causes:

1. **vm_call passed an absolute window base as a relative callee index** —
   `call_value(reg, base, base, ...)` read `data[base + base]`. every prior
   ok_call in the project's life ran from C with an empty call stack, so
   base==0 made `0+0` accidentally correct. the game's first callback with a
   live call stack was the first time it was ever wrong.
2. **the error path itself crashed**: runtime_error reset
   `call_stack.count = 0`; call_native still returned true; the OP_CALL
   handler reloaded `frame = data[count - 1]` = `data[UINT32_MAX]`. the fix —
   an `error_pending` flag consumed only at the outermost C boundary — turned
   out to be the seed of the future error model (pcall).
3. **stale-window GC trace**: call() raised `top` to frame_top without
   clearing the callee's not-yet-written window; dead pointers from old frames
   got traced under aggressive GC.
4. **latent argv invalidation**: call_native handed natives `data + p_callee +
   1`; a callback-driven register-file growth (realloc) would dangle it.

verification: 3600-frame soak x4 build variants (gcc, aggressive, asan,
asan+aggressive), all identical: 4912 foreach calls, 3467 closure callbacks, 4
death->retry->restart cycles, exit 0. plus a permanent embedding regression
test (native_apply + error-through-callback + post-error recovery) that
segfaults on the pre-fix binary.

## phase 6 — done (today)

owner ran the real game natively: fully works. web via emscripten: works (fps
wobble noted, not blocking). debug + aggressive GC + ASan: clean, 1000+ fps on
an old machine, gc-only spikes. v0.0.4 ships. benchmarks: register averages
~1.9x over the stack VM, sits at parity with lua on calls/closures/recursion,
beats lua on globals 1.52x, and every remaining gap is identified codegen
shape with scheduled fixes. see PROJECT_STATE.md for the table and analysis.

## the lessons (the part worth rereading in a year)

1. **green tests != working VM.** 109/109 passed while the game crashed four
   different ways. the harness tested the language; only real programs test
   programs. differential runs (stack vs register) + the game soak become
   permanent CI citizens. "done" = the old game runs.
2. **error paths are code too.** the segfault was IN the error reporter. every
   error path deserves a test that actually travels it.
3. **latent bugs hide behind accidentally-correct invariants** — base==0,
   A==0, "both sides hold the same stale bits". the cure is explicit
   contracts (okchunk.h) plus assertions that actually assert (mind NDEBUG).
4. **NaN-boxing + non-strict checks = booby trap.** tags share space with
   double payloads; STRICTLY or bust. someday make STRICTLY the default.
5. **GC rooting is a windowing discipline, not a patch set.** the rule that
   caught three of the four hardest bugs: *every value that is alive must be
   inside a rooted window at every allocation point*. write it on the wall.
6. **repro harnesses beat reasoning.** repro/repro2/repro3 (binder-faithful,
   no-raylib, input-driving) found every game bug faster than reading code,
   and they outlive the bugs — keep them runnable forever.
7. **the build matrix is an instrument.** plain / aggressive / asan /
   asan+aggressive each caught bugs the others couldn't see. keep all four.

## artifact index

- patches (in apply order): oklang-register-fixes.patch (phase 1, ~30 items),
  gc-and-leak-fixes.patch (phase 2), dorg3-boot-fix.patch (phase 3),
  load-null-fix.patch (phase 4), native-callback-fixes.patch (phase 5),
  vm-global-trace.patch (optional instrumentation).
- harnesses: download/dorg3-integration-harness/ (repro.c, repro2.c, repro3.c
  + stubs/values includes generated from binder.cpp).
- phase-2 deep dive: download/PORT_README.md.
- specs wave 1: download/docs-oct24/. state/roadmap/decisions wave 2:
  download/docs-oct25/.
- still open: exit-time leaks (~182KB), mixed-type `==`, varargs design
  (D6 sketch), disassembler rewrite (v0.0.6), register-file clamp,
  REPL locals persistence.
