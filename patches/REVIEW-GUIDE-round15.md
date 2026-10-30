# round 15 — rooting API + public exposure (patches 12–15)

apply order: **12 → 13 → 14 → 15** (all verified sequential on bc272e3).
verification: e2e 211/211 × plain/asan/asan-aggr, embed test × plain/asan/
asan-aggr, fresh-clone sequential apply + rebuild + full gate. the embed test
now actually runs — read §2 before anything else.

---

## §0 the answer to "does it deserve a draft of its own?"

no — agreed with your call. the rooting api is ~250 lines of engine change and
the patch IS the reviewable draft. what a markdown spec would have covered
(the contracts the diff can't say out loud) lives in §3 here instead. if any
contract feels wrong to you, that's a patch comment away, not a doc rewrite.

## §1 what landed, per patch

### patch 12 — gc rooting + traced natives (283 ins, 7 files)

the dorg3 review's "GC rooting hole" and the aggressive-gc postmortem's
"internal twin" observation, closed:

- `ok_native_value_new(ok*, void*, ok_native_destructor, ok_native_tracer)`
  — **breaking signature change** (was 3 args). `ok_native_tracer`:
  `bool (*)(ok*, void*)`, called during GC trace while your native value is
  being traced. mark every ok_value your structure still holds via
  `ok_gc_mark_value(ok*, ok_value)`. must not allocate (collector is
  mid-walk). NULL tracer = the old opaque behavior, so dorg3's existing
  values port with a `, nullptr`.
- `ok_gc_mark_value` — **only valid inside a tracer callback**. enforced by
  a new `gc.is_tracing` flag set/cleared around `trace()`; any other call
  site returns false instead of stranding gray entries the next collect
  would walk against swept objects.
- `ok_root(ok*, ok_value) -> uint32_t` / `ok_unroot(ok*, uint32_t)` —
  embedder-owned strong references. growable array on the **raw allocator**
  (rooting must never itself trigger a collection), null-slot + LIFO
  freelist recycling, ids are slot+1, 0 = `OK_ROOT_INVALID`.
- roots are marked in `mark()` alongside guards/globals/registers.
- `trace_object` grew the `OBJ_NATIVE_VALUE` case (previously absent — the
  type simply had nothing to trace).

### patch 13 — array public api (145 ins, 3 files)

the gap your dorg3 binder worked around with fork-arrays:

- `ok_array_new(ok*, const ok_value*, count)` — mirrors OP_ARRAY_NEW's
  guard pattern: the fresh array is store-linked but unreachable while
  `value_array_append` grows through the gc-aware allocator; each host item
  is guarded across its own append (host locals are invisible to the
  collector, unlike the VM's register-rooted elements).
- `ok_value_is_array` / `ok_array_len` (non-array → 0) / `ok_array_get`
  (oob → null) / `ok_array_set` (oob → false, no store) / `ok_array_push`
  (guards array + value; native_push's no-guard comment only holds because
  ITS arguments are register-rooted).

### patch 14 — print capture hook (83 ins, 5 files)

`OP_PRINT` used to `value_debug_print` + `puts` straight to stdout — hosts
could not capture output. the playground needs this. `ok_set_print(ok*,
ok_print_fn, void*)`: the callback receives the **raw value**, the host
formats via `ok_value_as_*`. default (NULL) keeps CLI behavior byte-identical
(e2e 211/211 proves it). the hook lives on `struct vm` (ok.c's `struct ok`
is private; OP_PRINT has `p_vm`). the printed value stays register-rooted
during the callback, so a host callback that allocates is safe. the test's
lambda replicates the CLI's exact formatting (%g / true/false/null / raw
bytes / `<object>`) — that's the documented recipe for embedders.

### patch 15 — ci aggressive lane (4 lines)

adds `aggressive-asan` to the ci matrix: `-DOK_ENABLE_SANITIZERS=ON
-DOK_AGGRESSIVE_GC=ON`. this is the lane that catches the 167-NOTOK class;
plain+asan both missed it. wall time cost: the e2e suite collects on every
allocation — expect the lane to run a few minutes.

## §2 the embed test was red on master

`tests/integration/embedding/src/main.cpp` still used `if a != 44? print...`
— the no-question merge swept the e2e corpus but missed this file. verified:
parse errors, `assert(result == OK)` aborts, **CI's embedding-test step on
bc272e3 fails** (check the actions tab — it should be red right now). patch
12 fixes the syntax as part of extending the file. this is also why the
aggressive lane matters: nobody ran this test locally in any gc config, or
the `?` would have been caught at merge time.

## §3 contracts worth reading twice (the actual draft)

**tracer**: called with the collector mid-walk. no allocating, no ok_run, no
ok_array_push. mark children with ok_gc_mark_value; return false to
propagate failure. cycles are safe (mark-before-trace means a self-holding
structure terminates). root the native value (or otherwise keep it
reachable) BEFORE populating children — the tracer sees your structure's
current state at every collection, which is what makes incremental
population safe.

**roots**: an id is valid until its ok_unroot returns true. ids recycle
LIFO, so a stale double-unroot can silently unroot a *different* value —
guard ids like pointers (lua's luaL_ref has the same shape and the same
footgun; wrap them in RAII on the C++ side). rooting null is rejected
because a null slot is the free marker; numbers/bools root fine and are
kept verbatim. after ok_unroot, the value may be swept by the very next
allocation — the test demonstrates this the hard way (the gate caught me
touching an unrooted value; the fix is in the test and the comment is the
contract).

**arrays**: ok_array_new/push guard their windows internally; keeping the
array or pushed values alive afterwards is the host's job (root, traced
native, global, or registers).

## §4 audit: what else is still unexposed (and why I stopped here)

| internal | exposed? | call |
|---|---|---|
| arrays (new/push/get/set/len) | **now yes** (13) | the binder-2 blocker |
| print capture | **now yes** (14) | the playground blocker |
| traced natives + roots | **now yes** (12) | the map blocker |
| ok_call multi-return | no | vm_call still has the `TODO multiple returns`; exposing a half-feature pins the shape |
| `as` type tags (TT_STR etc.) | no | they're the temporary-type mechanism; hosts injecting real types should wait for the error model so failures are reportable |
| value_debug_print formatting | no (documented recipe in ok.h instead) | keeping one formatter in the CLI path avoids drift; hosts copy 10 lines |
| len/push/pop/remove as C fns | no | they're globals — hosts already call them via ok_call, and C-side `ok_array_len` covers the read case |

one nit for your next comment pass, not patched (your call):
`ok.c:55`'s "it never triggers a collection" — value_array growth through
the tracking allocator *does* collect (okvm.c's OP_ARRAY_NEW comment says
so, and patch 13's guards rely on it). the native_push conclusion is still
right (register-rooted window), but the stated mechanism is the wrong half.

## §5 maps, on top of this

your map native is now buildable as: OBJ-style native value holding a
std::unordered_map<ok_value, ok_value> (or your own open addressing since
ok_value is a great hash key), tracer marks keys+values, destructor frees.
key semantics (what hashes: string bytes vs identity) is a language-level
ruling, not an api one — flag it when you get there.
