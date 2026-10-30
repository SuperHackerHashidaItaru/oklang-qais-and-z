# postmortem: the asan + aggressive-gc mass failure on no-question (0a88601 + patch 09)

## symptom

- asan + `OK_AGGRESSIVE_GC`: **[39 OK], [167 NOTOK]** (Q's tree; 172 here — mine
  includes the 5 `operators/as/` tests that patch 09's README promised but never
  shipped, hence patch 11).
- plain `-O2` and asan-only: 210/211. the failure only exists when the GC
  actually collects during the window below.
- two crash shapes, same corpse:
  - Q's: `SEGV on 0x0` at `okvm.c:1373` (`word instruction = READ_WORD()`) —
    the **first fetch of the top-level frame**, `ip == NULL`.
  - mine: ASan `heap-use-after-free` in `mark_object` during
    `create_object_closure`'s allocation (`vm_interpret:111`), on the 160-byte
    `object_function` that `begin_context` allocated during compilation, freed
    by `sweep` during `ok_get_top_level_values`.

## root cause

the branch inserted a new allocating phase between `compiler_compile` and
`vm_interpret`: `ok_get_top_level_values` (ok.c) fetches the 7 core values via
`ok_global_get`. every `globals_store_get` (okglobals_store.c:82) builds a
**fresh hashed key string** (`create_hashed_string_hash` →
`create_string_from_string_view` → `create_dynamic_string`) — an allocation per
lookup, 7 per run.

under `OK_AGGRESSIVE_GC` every allocation collects. at that moment the compiled
top-level function is reachable **only** from `compile_result.function` — a C
local, invisible to the GC. (mark roots: vm registers `[0, top)` + call-stack
closures + open upvalues; compiler contexts — **empty once compile returns**;
globals; the single guard. none hold it.) so it gets swept.

`vm_interpret` then `gc_guard_up`s the freed pointer. from there, quarantine
luck picks the crash shape:

- the block is still in ASan's quarantine → `mark_object` reads freed memory →
  heap-use-after-free (my run).
- the block was already recycled into a fresh zeroed allocation →
  `chunk.code.data` reads `0` → `frame.ip = NULL` → SEGV at the first
  `READ_WORD` (Q's run).

master/v0.0.5 never had this window: `ok_run` went straight from
`compiler_compile` to `vm_interpret`, whose first act is the guard. the
staging step is new on this branch.

clean signature: **all 39 pre-fix passers are `expect error` tests** — they
exit at parse/compile before the window. every test that reached bytecode died.

## fix (patch 10)

1. `ok_run` guards `compile_result.function` across the whole gap (and checks
   the guard's return — an unrootable function must not run). `vm_interpret`
   keeps its own guard; they now compose instead of colliding.
2. the guard became a 16-deep **stack** instead of a single value (retires the
   `TODO array based gc guard` at vm_interpret:108):
   - nested pairs compose. with one slot, an inner `gc_guard_up` silently
     fails and the inner `gc_guard_down` releases the *outer* guard early —
     the compile-phase pairs (`begin_context`, the `compile_function_decl`
     tail, `ok_global_define`) survive today only by being carefully
     sequential.
   - underflow `gc_guard_down` is a no-op rather than releasing someone
     else's guard.
   - `mark()` walks all live slots; overflow returns false.

`shadow_len_does_not_invalidate_for_in.ok` updated to the ruled semantics
(Q: *glob let len was a past assumption from when len was global; the compiler
loads `' len'` directly, it is ok*): for-in iterates under a user
`glob let len`, and `len(arr)` still calls the builtin. the old `expect: 67`
encoded master's plain-global `len`; printing the native instead embeds a raw
pointer, so the test asserts behavior, not formatting.

## verification

- sandbox matrix, real e2e runner: **plain `-O2` / asan / asan+aggr → 211/211
  each** (211 = your 206 + patch 11's five `as/` tests).
- fresh clone `0a88601` → 09 → 10 → 11: applies clean, **211/211 on plain and
  on asan+aggr**.

## follow-ups (not in this patch)

- `ok_get_top_level_values` could cache global ids instead of hashing a fresh
  key string per lookup per run: kills 7 transient allocations per `ok_run`
  and the whole bug class at that site. the guard stays as the safety net
  either way.
- optional: warn when a `glob let` name is swallowed by the hidden core scope
  (`glob let len = 67` is currently silently unreachable from the body).
- the embedding-facing twin is still the roadmap item from the dorg3 review:
  an explicit rooting protocol (`ok_value_retain/release` or registered roots)
  for host-held values. this crash is the same disease on the inside — every
  C-local handoff of a GC value is a sweep waiting for a collection. imports
  will multiply these handoffs (each import is another compile → load → run
  boundary), so the discipline should be systematic before then.
