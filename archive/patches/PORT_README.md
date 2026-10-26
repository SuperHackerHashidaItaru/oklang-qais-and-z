# OKLang register-VM port — session log & fix readme

Branch: `register` (base commit `8e0dc55` "apply fixes.")
Date: 2025-10-25 session
Patch: everything below is also in `gc-and-leak-fixes.patch` (5 files, +49/-13)

## TL;DR — where we are now

| configuration | before this session | after |
|---|---|---|
| plain build | 109/109 | **109/109** |
| `OK_AGGRESSIVE_GC` (collect on every alloc) | 105/109 (4 segfaults) | **109/109** |
| ASan (cmake `OK_ENABLE_SANITIZERS` recipe) | 11/109 (98 leak-exit failures) | **109/109** |
| ASan + aggressive GC combined | (not runnable) | **109/109** |
| integration: embedding API test | fake "OK" (asserts compiled out) / segfault with asserts on | **passes with asserts on, clean under ASan** |

Every failure class had a *different* root cause. The aggressive-GC crashes were two separate bugs,
the ASan catastrophe was one systemic leak pattern, and the embedding test hid a fourth bug behind
disabled asserts. Details below, in the order they were hunted.

---

## 1. The four `*_long` crashes under `OK_AGGRESSIVE_GC` (exit 139, no output)

Crashing: `local_long`, `upvalue_long`, `constant_long`, `global_long`. Passing: `jump_long`.
Reproduced on gcc 14.2 identically to your clang run, so it's logic, not toolchain.

### Bug 1 (the segfault): non-strict tag checks in the GC marker

`OK_AGGRESSIVE_GC` is a **compile-time define** (CMakeLists option), so the failing runs were a
different binary — that's why your numbers diverged from mine.

With collection on every allocation, `mark_value` walks constants/registers/globals constantly.
The new NaN-box layout puts tag bits at **bits 48-49 + bit 63** — inside the payload space that
raw doubles also occupy. A non-strict `IS_VALUE_OBJECT(v)` is just a tag-bits comparison, so
perfectly ordinary doubles **fake-decode as objects**. The killer examples from the actual crash:

- `19.0` = `0x4033000000000000` → tag bits read `0x0003...` = `TAG_OBJECT` → payload `0x300000000000`
- `39.0` = `0x4043800000000000` → same → mark_object derefs garbage → SEGV

Only doubles whose 4th-from-top nibble is 3/7/b/f alias the object tag — that's 19.0, 23.0, 27.0,
31.0, 38.0, 39.0, … — which is why only the `_long` tests (257 distinct numeric constants) hit it,
and `jump_long` (jumps only, no big constant pool) survived. ASan pinpointed it: crash inside
`object_is_marked ← mark_object ← mark_value ← mark_value_array ← trace_object(OBJ_FUNCTION)
← trace ← collect ← allocate_impl ← string_init ← add_local`.

**Fix** (`src/okgc.c`): `mark_value` now uses `IS_VALUE_STRICTLY_OBJECT` (the QNAN-aware variant
that already existed in okvalue.h for exactly this reason).

Same class of bug, same session, these sites were also switched to strict checks:

- `okgc.c` `gc_guard_up` — a raw double like 5.0 fake-decodes as non-null → guard silently rejected
- `okobject.h` `value_is_object_type` — would `OBJECT_TYPE()`-deref a fake pointer; this also fixes
  the public `ok_value_is_string` predicate
- `ok.c` public predicates `ok_value_is_null/_bool/_object` — `ok_value_is_null(1.0)` returned
  **true** before (1.0's tag bits are zero = TAG_NULL)

The `perform_*` operator chains in okvm.c are *not* affected — they test `IS_VALUE_NUMBER` first,
which catches every raw double before any tag check runs.

**Design note for later (your call, not applied):** the long-term cure is making the base
`IS_VALUE_OBJECT`/`IS_VALUE_NULL`/`IS_VALUE_BOOL`/`IS_VALUE_INT` macros strict themselves and
deleting the STRICTLY variants — one extra `& QNAN` per check, and the ordering-dependent safety
in the operator chains stops being load-bearing. I left the macros alone to keep this patch
function-level; every *caller* is now safe either way.

### Bug 2 (the remaining 2 crashes): closure written *at* `top`, rooted only in `[0, top)`

After the tag fix, `local_long`/`upvalue_long` still crashed. ASan gave a textbook
use-after-free:

```
freed: sweep ← collect ← reallocate_impl ← register_array_grow ← call
used:  call reads p_closure->function->registers
alloc: create_object_closure ← vm_interpret
```

Sequence: `vm_interpret` writes the fresh closure into `reg->array.data[reg->top]` — the slot
**at** index `top`. `mark_vm` roots `[0, top)` — exclusive. The single-slot `gc_guard` had already
been released (`gc_guard_down` right after the write). Then `call()` grows the register file
(only happens when `frame_top > 256`, i.e. exactly `local_long`/`upvalue_long`) → that
`register_array_grow` allocation triggers GC → the closure is unmarked → swept → `call()`
immediately dereferences the freed closure.

The old stack VM never had this because pushing onto the stack *increments* top, so the value was
inside the root range immediately.

**Fix** (`src/okvm.c` `vm_interpret`): `reg->top += 1;` right after writing the call register.
`call()` overwrites `top = frame_top` anyway, so the bump only extends the root window across the
gap. One line + comment.

### Bonus latent bug fixed while in there (`okvm.c` `call`)

The register-file grow path null-filled fresh slots starting from `array.count` — but the register
file is written directly (`data[i] = ...`), **never appended to**, so `count` is always 0. A nested
call that grows the file would have NULL-wiped the caller's entire live register window. Now
null-fills from the old `capacity` (the real initialized watermark). Nothing in the current suite
trips this (only the main frame ever grows the file past warm capacity), which is why it survived.

---

## 2. The ASan horror (11 OK / 98 NOTOK, "exploded in the terminal")

Fully reproduced with the exact cmake recipe (`-fsanitize=address -fno-omit-frame-pointer`, no
UBSan, no extra options). Findings:

- **Zero memory-corruption reports.** All 98 failures were **exit code 1** — LeakSanitizer's
  "detected memory leaks" exit status. No crashes, no UAF, no overflows.
- The 11 passes were exactly the `// expect error` tests (they expect non-zero exits, so a
  leak-flipped exit code doesn't change their verdict) plus `closure_loop.ok`.
- The e2e runner (tests/e2e/src/main.cpp) pipes stderr per test, so the "explosion" was 98 full
  leak reports interleaved — nothing was actually wrong with your ASan invocation.

### Root cause: `*_pop()` never runs the element deinit

The compiler's arrays are declared with real element deinits —
`ARRAY_DEFINE_DEFAULT(scopes, scope, scope_deinit)`,
`ARRAY_DEFINE_DEFAULT(compile_contexts, compile_context, compile_context_deinit)`,
`loop_stack` with `loop_context_deinit` — but the generated `name##_pop()` is a bare `count--`.
So every pop leaked the popped element's owned memory, and at `compiler_deinit` time `count` was
already 0, so the array-level deinit had nothing left to walk. Compiling *any* program leaked:
every scope's hash table (with malloc'd hashed-string name keys), every loop's continues/breaks
arrays, every finished function context's scopes/upvalues/loop_stack/register-metadata arrays.

**Fixes** (`src/okcompiler.c`):
- `end_scope`: `scope_deinit(scp, ...)` before `scopes_pop`
- `end_context`: `compile_context_deinit(...)` before `compile_contexts_pop`
- new `pop_loop_context(ctx, alloc)` helper used at all four loop-pop sites
- `compile_function_impl`: deinit the function's compile context before popping it (safe because
  the upvalue descriptors were already copied into `proto->upvalues` — the runtime reads them
  from `object_function`, never from the compile context)

After these: the whole suite runs **leak-clean under LSan at default settings**. The object side
was already fine (`object_store_deinit` frees everything; `chunk_deinit` covers code/constants/
identifiers/source_info).

---

## 3. The embedding/integration test ("this ok could highly be fake lol")

You were right — it was fake. Two independent problems:

1. **Your build had asserts compiled out** (Release/NDEBUG strips `assert()`), so
   `assert(result == OK)` after the failed `ok_run` was a no-op and the test printed
   "embedding API: OK" and exited 0 regardless. Worth fixing the harness later: either force
   asserts for test targets or replace them with explicit `if (!x) return 1;` checks.
2. **The `undefined variable: host_add` errors did not reproduce** on current `8e0dc55` + this
   patch — the compiler's `resolve_variable` correctly falls back to the shared `globals_store`
   (locals → upvalues → globals → error), and host functions registered via
   `ok_global_define_native_function` before `ok_run` resolve fine. Your failing run was almost
   certainly a stale test binary linked against an older lib. Rebuild and rerun.
3. **The real bug hiding underneath:** with asserts enabled, the test segfaulted in `ok_call` on
   a native function. `call_native` did
   `frame = &call_stack.data[count - 1]` — but natives never push a call frame, and via `ok_call`
   the stack is *empty*, so that's `data[-1]`, a wild pointer. Worse, even in the bytecode path
   it wrote the result to the caller's `return_base` instead of the call window start.

**Fix** (`src/okvm.c` `call_native`): the native's result now goes to `reg->array.data[p_callee]`
— the callee slot, exactly matching your documented `OP_CALL` semantics
(`r[a]... = r[a](r[a+1]...)`), and `reg->top = p_callee` as before. This also makes
bytecode→native calls correct (they were completely untested: e2e has no native functions,
`print` is an opcode).

With the fix the full embedding test passes **with asserts on, clean under ASan**: value
predicates, host string creation, global define/get, the embedded script (host natives visible to
compiled code), `ok_call` on script functions (`multiply(6,7)`), `ok_call` on natives, arity
errors, `ok_native_error` propagation, and native-value destructors.

---

## 4. NaN-boxing review (okvalue.h) — findings from this hunt

- The tag layout (T3 at bits 48-49, T1 = sign bit, QNAN pattern for "not a number") is sound and
  gives you null/bool/object/int/user + 2 reserved tags with a 48-bit payload. The INT sign
  extension trick in `payload_to_int` is correct.
- **The one systemic sharp edge:** tags live in double-payload space, so *every* unqualified tag
  test is a landmine that silently lies for specific doubles (null: tag bits 0; object: nibble
  3/7/b/f; int: bit 63 set → all negative doubles fake-decode as INT! e.g. `IS_VALUE_INT(-1.0)`
  is true — check any non-strict INT sites you add in the future). The STRICTLY macros are the
  safe defaults; consider making them THE macros (see design note above).
- `IS_VALUE_NUMBER` is inverted-relative-to-tags (true for everything except exact-QNAN-pattern),
  which makes it the correct first check in cascades. Keep that ordering convention.
- One asymmetric detail to be aware of: `VALUE_AS_BOOL` on a non-bool returns false (tag compare),
  never garbage — fine.

---

## 5. How to reproduce / verify everything

```sh
# plain, aggressive, asan (mirrors cmake option flags exactly)
cmake -B build -DOK_AGGRESSIVE_GC=ON   # etc. — or the three gcc builds I used
ctest / your usual runner

# my gcc equivalent (sandbox has no cmake):
scripts/build_ok.sh all        # builds build-gcc, build-aggressive, build-asan
build-gcc/e2e_runner build-{gcc,aggressive,asan}/ok tests/e2e/tests

# the combo that stresses hardest (asan + collect-on-every-alloc):
gcc ... -DOK_AGGRESSIVE_GC -fsanitize=address -fno-omit-frame-pointer
```

## 6. Still open / notes for later

- **Register-file clamp** (`okvm.c` `call`): `new_capacity` clamps at `UINT16_MAX + 1`, but
  `frame_top = callee + function->registers` is not bounded by that — deeply nested calls with
  big frames could still set `top` past capacity → OOB writes. Needs a design decision
  (error out vs. unclamped file) — not triggered by any current test.
- **okchunk.h comment drift** (OP_SUBTRACT/ADD family): still pending your doc patch; okdebug.c
  rewrite still deferred behind it.
- **e2e runner hardening** (your list, agreed): per-test timeout, failure buckets
  (compile-error/wrong-output/crash/timeout), valgrind/asan modes with `detect_leaks` as a
  first-class configuration, parallelism, and don't let expected-error tests mask leaks.
- **Integration harness**: make assertions build-type-proof (custom assert or explicit checks).
- Local-first REPL warning idea (Lua-style, but inverted since locals are the default): worth
  doing when the CLI round happens.
