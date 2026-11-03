# oklang — decision log (2026-10-25 wave)

format: decision / context / rationale / revisit trigger. statuses: LOCKED
(owner confirmed), PROPOSED (Z recommends, owner hasn't ruled), OPEN (parked).

---

## D1 — register VM is primary; stack VM is the differential oracle [LOCKED]
the rewrite is done and validated by the game. the old stack VM stays available
(ok-stack binary / old branch) precisely so every script can be run under both
with outputs diffed. it caught the bugs the harness missed; it becomes a
permanent CI citizen (ROADMAP v0.0.5).

## D2 — globals: compile-time, index-based [LOCKED, benchmark-confirmed]
names resolve at compile time to indices into an array-based store; unknown
name = hard error. no name lookup at runtime, ever (this beat lua 1.52x on
global_access). a name-based dynamic path with inline caching comes later as
an explicit escape hatch — it will be slower and that's fine, it's not the
default. no string interning anywhere for now; maps v2 caches hashes in string
headers instead.

## D3 — numbers: f64-only scalars through v1 [PROPOSED — Z's one real pushback]
owner assumed int support now ("you didnt say anything about int type support
so i would assume we should support it now"). Z says: don't.
- TAG_INT already exists in the value model but the compiler never emits it —
  keep it that way (internal use only, if ever).
- what a second scalar type costs: coercion rules on every operator, overflow
  semantics, `1` vs `1.0` print/format ambiguity, mixed-type `==` (already an
  open sore), every builtin signature, double the benchmark surface. lua 5.3's
  int/float split created a decade of gotchas; JS never left f64 and thrived.
- what it buys today: nothing a 2D app/game lang needs. f64 is exact for
  integers to 2^53 — the whole domain fits.
- where ints ARE real and WILL come: array lengths/indices (i32/u32 internal,
  never user-visible as a type) and, when the engine needs binary data
  (audio/vertex/pixel), **typed array buffers** (i8/u8/.../f32/f64 storage
  flavors; values read out are still numbers). that's the revisit trigger:
  engine assets, not language design.
- numeric-for consequence (LOCKED regardless of D3): the oct24 spec's
  int/float mode split is DROPPED — always f64 induction; step ±1 loops are
  exact to 2^53; fractional steps documented as C-double behavior. FOR_PREP/
  FOR_LOOP become 2 opcodes instead of 4.

## D4 — arrays [LOCKED with deltas]
oct24 spec stands (contiguous GC-tracked value arrays, ARRAY_NEW/GET/SET/LEN
ops, natives push/pop, OOB = loud runtime error, identity `==`) plus:
- **no slicing** — user wants a slice, they for-loop into a new array.
- fake ranges stay until a real range protocol (they'll look identical).
- natives len/push/pop NOW; `.len`/`.push`/`.pop` methods LATER (post-classes,
  when primitives share one class object).
- the trace_object case for OBJ_ARRAY is the bug class of the whole rewrite —
  it's in the spec's tests, keep it there.

## D5 — lvalues / write targets [PROPOSED]
is the architecture scalable? yes — register VMs are the good case — but the
compiler needs a **place** abstraction so reads and writes compile through the
same resolution and can't drift (the dorg3 bug class). ladder:
LOCAL reg -> UPVALUE -> GLOBAL slot -> INDEX(base, idx) -> FIELD(base, offset).
rules that make it scale: nested targets (`a[i].x = v` later) evaluate the
address chain ONCE into temps, then a single store; never re-evaluate.
near-term (arrays landing now): the minimal version — a 3-temp discipline for
`m[i] = v` (base once, index once, value once, then ARRAY_SET). full place-IR
when classes land; don't over-build before the second consumer exists.

## D6 — varargs + multi-return [PROPOSED — answers "is the compiler arch good for it?"]
yes, unusually so — argument windows and return windows are just register
ranges. decisions to lock:
- **calling convention**: OP_CALL gets argc (and later a result count).
  callee R0 = closure slot, params R1+ (existing). NEW: `frame->argc` becomes
  a runtime field set by call() — this single change dissolves the old
  "OP_CALL p=1 argc-from-top conflicts with top=frame_top" wrinkle recorded
  during the port.
- **multi-return**: callee executes RET A,n writing n consecutive registers
  starting at its base. since the callee's base IS the caller's call window
  start (r[a]), results land at r[a]..r[a+n-1] with **zero copy** —
  destination-passing falls out of the window convention for free (consistent
  with the existing OP_CALL contract `r[a]... = r[a](r[a+1]...)`).
- **static result arity**: protos declare how many values they return (like
  arity); call sites hard-fail at COMPILE time when the proto is known
  ("expected 2 returns, got 1") — same doctrine as globals. destructuring:
  `let (x, y) = f()`.
- **varargs**: `fu f(a, b...)` — trailing args already sit in consecutive
  registers; prologue op VA_PACK materializes them into an array local. v1
  always packs (one alloc per call — fine); lazy window-views + pack-on-
  escape is a later optimization. spread `g(b...)`: CALL_SPREAD copies the
  array's elements into the arg window then invokes with argc = fixed + len.
  spread can be deferred past v1 if unused.
- what the two-pass compiler must add: arity/result checks in the semantic
  pass, destructuring patterns in let, VA_PACK/CALL_SPREAD in codegen. nothing
  architectural blocks it.

## D7 — error handling: lua-model values + bytecode handler frames [PROPOSED]
owner asked: exceptions (old cpp draft) vs something coro-centered (wren)?
answer: neither — **lua-model**, and it's coro-friendliest by construction:
- errors are plain values (any value can be an error; `error(v)`/`throw v`).
- pcall-equivalent as the primitive: a frame flag "handler boundary". on
  runtime error: pop frames to the nearest handler, hand it the value + a
  lazily-captured traceback. because the VM is one C dispatch loop, this is
  ~50 lines and needs **NO setjmp/longjmp** — which matters because wasm's
  longjmp story is expensive/awkward. (PUC lua needs setjmp only because its
  pcall is a C function; ours is bytecode.)
- coros: each coro owns its call stack, so unwinding is local. a coro that
  errors with no internal handler is marked dead-with-error; resume() returns
  a status + the error value (explicit rethrow, no cross-coro surprise).
- the error_pending flag machinery from the native-callback fix IS the seed of
  this — error propagation through nested natives already works.
- try/catch sugar compiles to the same handler frames (post-classes).
- why not wren-model: it couples error identity to class objects; we have no
  classes yet and shouldn't front-load class decisions into the error design.
  why not exceptions: needs a type system; and `return null, err` works day
  one via multi-return + value-errors anyway.
- dtor/finally ordering during unwind: pinned when classes land (D10).

## D8 — pipeline isolation: snapshot-swap commit [LOCKED on owner's variant]
owner's refinement of the oct24 spec: the compiler snapshots the globals store
into its own copy at compile start, mutates the snapshot freely, and on a
clean compile swaps the owning pointer — zero merge pass, zero second copy at
commit. Z's audit: green light, it's cleaner, and it KILLS the multi-insert
map requirement. invariants that make it safe:
1. **append-only indices** — every previously compiled function addresses
   globals by index; the snapshot must preserve committed indices and only
   append. (also makes "check duplicate names against BOTH states" free:
   looking up in the snapshot IS checking both, since snapshot ⊇ live.)
2. **snapshot is a GC root during compile** — the compiler allocates, gc can
   fire mid-compile (the lesson of the whole rewrite: anything alive only in
   a staging slot must be rooted at every allocation point).
3. **one ownership pointer** — the swap is a pointer replacement at a single
   owner; no other code holds store pointers across a compile.
4. failure path: deinit the snapshot, live state byte-identical (the
   pipeline-isolation spec's invariant, unchanged).
costs: O(globals) clone per compile (fine at our scale); values shared between
live+snapshot (fine — non-moving gc, double-marking is idempotent). caveat:
compiles happen while the VM is quiescent (single-threaded); true hot-reload
mid-execution is a v2 question, not this.

## D9 — modules: compile-time, zero runtime cost [LOCKED on shape, details PROPOSED]
- module names resolve at compile time. imports: `import x`, `import x as z`,
  `from x import y` (python-like semantics).
- a module's exports are a **slot array** with a name table — the same
  index-based trick as globals. `x.y` after `import x` compiles to a slot
  load; zero runtime cost for module usage, exactly as the owner intended.
- **top-level code runs once**, on first import; result cached in the module
  registry (keyed by interned path). the registry IS the snapshot-swap
  pipeline's client — compiling a module is compiling a TU against a snapshot.
- **no `__main__` magic**: the entry file is a plain module whose top-level
  runs after its imports. an `is_main()` builtin only if a real case appears.
- circular imports: hard compile error in v1 (hard-fail doctrine).
- **dynamic import** (`import(name)` with runtime name) returns the module as
  a VALUE; all access through it is name-based dynamic lookup — the future
  dynamic-global path + inline caching applies. two worlds, chosen by syntax
  at the use site; the usual case stays the fast case. contained blast
  radius, exactly the owner's instinct.

## D10 — classes: frozen shape + copy-down + metaclasses + IC [LOCKED on vision]
owner's sketch (recorded in PROJECT_STATE direction + their code sample):
frozen fields (`let`, `let mut`, `let unset_member` settable only in ctor),
single inheritance via copy-down, metaclasses = statics + class-base
overrides, no dynamic expansion, inline caching, primitives share ONE class
object per type (so `''.join()` and `1.to_str()` cost no allocation).
Z's additions / flags:
- `let unset_member` -> **compiler-enforced definite initialization**: every
  ctor path must assign it or the TU fails. cheap to enforce, huge safety win,
  very on-doctrine.
- `let member = 1` (non-overridable) compiles into the **shape template**:
  instantiation = alloc + memcpy of frozen defaults + run ctor for unset/mut
  fields. copy-down inheritance and this are the same machinery.
- **dtor is the one landmine**: with gc, dtors are non-deterministic and not
  guaranteed to run (especially at exit). the owner's `static_counter--`
  sketch implies deterministic lifetimes. doctrine: dtors are BEST-EFFORT
  (document it); anything that matters gets an explicit close()/defer.
  refcounting is rejected (cycles + gc interplay = pain forever).
- inline caching: fields = static offsets (frozen shapes), no ic needed; only
  polymorphic METHOD calls through base refs need one — single inheritance =
  monomorphic ic = one class-pointer check. cheap and fast. owner's "we win
  before even adding them" vs hash-table languages: justified.
- OPEN: **implicit self** — the owner's sample uses bare `member_2` inside
  methods. must pin the resolution order vs locals/upvalues/globals. Z
  suggests: params/locals shadow members; bare member names resolve otherwise;
  globals lose to members. needs a deliberate decision (it touches the whole
  compiler resolution chain).

## D11 — coroutines: stackful, lua-like [LOCKED on shape]
per-coro register file + call stack; yield/resume switch between coros; the C
game loop may resume (C -> ok -> C is fine, proven). yielding THROUGH a native
frame is impossible by construction — runtime guard on native_call_depth (the
counter already exists). scheduler is NOT part of this (D12).

## D12 — scheduler: tiny in-house C lib, NOT core, NOT libuv [PROPOSED, owner-aligned]
owner's plan: write a tiny asyncio-style C library and use it for the language.
Z's audit: right call, and the burden is real but bounded:
- core loop + epoll/kqueue/select backends + min-heap timers + prepare/check/
  idle hooks: ~1-2k lines C. wasm backend maps onto the browser loop
  (rAF/setTimeout/fetch completion) — which ALSO fixes the web fps wobble
  (render on rAF, gc slices in idle).
- VM-side requirement is small: coro suspend + "resume from C callback" —
  the reentrancy shape array_foreach already proved.
- the libuv mockery is about bulk and abstraction weight (and it being a
  build-time dep), not about event loops being wrong. a 2k-line own loop keeps
  the zero-dep embedding story. lua says "you build it"; we say "we ship it,
  separately, and embedders can ignore it".
- real cost estimate: the loop is weekends-scale; the hard part is API design
  (what await is: yield-to-scheduler + resume-with-value; a promise-ish value
  is just a coro + result slot). windows: select first, IOCP later.

## D13 — strings: in-house UTF-8, codepoint-based [LOCKED on shape]
python3-like, zero deps: codepoint API, cached codepoint count (immutable
strings), Go-style lossy decode (invalid byte -> U+FFFD, advance 1, never
crash/loop), O(n) indexing v1. identifiers ASCII in v1. interning stays OFF.
first version is deliberately boring; optimizations later.

## D14 — C-style for: freeze, don't drop [PROPOSED — recorded disagreement]
owner plans to drop it entirely in favor of numeric-for. Z: numeric-for
covers ~95% now, but keep the general for parsing (frozen, unoptimized,
maybe warning'd) — when an iterator protocol arrives post-v1 it becomes the
generic iteration loop and gets a real job. dropping syntax is easy later;
un-dropping is hard. owner decides; this is a one-line parser decision either
way, no urgency.

## D15 — LSP: monolithic != no-LSP; 3-phase plan [PROPOSED, evidence-backed]
owner's question: gcc gave up, odin refuses until a library rewrite — is
monolithic = no LSP a closed case? answer: no, but the concern is real.
- pyright: yes, a full reimplementation (own parser + binder in TypeScript);
  it does not use CPython's compiler at all. that's the "give up and rewrite"
  path.
- clangd exists because clang was designed as libraries from day one. gcc
  never was; gccjit exists but it's a niche struggle. odin's bill says the
  same thing: until the compiler is a library, no official LSP.
- the ACTUAL requirements: (a) error tolerance — never exit, recover, return
  diagnostics; (b) embeddability — no process-global state, no aborts,
  controlled allocation; (c) an incremental entry point. "rewrite" is one way
  to get there; "library-ize + harden" is another, and it's cheap for us
  because the parser-hardening round is already on the roadmap.
- our trump card: **tree-sitter-oklang already exists and works**. the
  standard modern architecture (rust-analyzer, zed, etc.) is tree-sitter for
  structure + compiler-as-library for semantics.
- phase 1 (any time, zero compiler changes): thin LSP server wrapping
  tree-sitter — highlight, folding, symbols, selection, diagnostics-lite
  (ERROR nodes). days of work.
- phase 2 (rides v0.0.5 parser hardening): library-ize — no exit(), a
  diagnostics list, arena allocation, multi-error parse recovery.
- phase 3 (post-v1): semantic server — check-mode compile per file, symbol
  tables for hover/go-to-def. and here the doctrine pays: compile-time
  globals and static module exports mean definitions are statically
  resolvable — no dynamic-scope chasing like python. the language design is
  secretly LSP-friendly.

## D16 — stdlib v1 shape [PROPOSED]
zero-dep, C, natives-as-globals first; re-homed into modules (with aliases)
when D9 lands. flat list:
- core: print, len, to_str, to_number, type-ish, assert, error, pcall (D7)
- math_: floor/ceil/round/abs, min/max/clamp, sqrt/pow/exp/log, sin/cos/atan2,
  pi/e, random (seeded, PCG or xorshift — tiny)
- string_: len/sub/find/split/join/replace/upper/lower/trim/starts/ends,
  codepoint/char
- array_: len/push/pop (natives now), index_of/contains/reverse/sort(with
  comparator)/copy
- fs_: read_file/write_file/exists/list (unblocks the static-site dogfood)
- time_: now/clock, sleep (coro-aware later)
- deliberately later: regex (post-v1, or never — a tiny backtracker), json
  (**written in oklang itself** as dogfood), datetime, threads (never?).
- the embedding surface (register natives, ok_call, native values) is already
  proven by dorg3's binder — that IS the host API, keep it stable.

---

## parking lot
mixed-type `==` (error vs false — decide in the error-model round). implicit
self order (classes round). REPL locals persistence. register-file clamp
overflow (known, untriggered). ordered maps. hot-reload semantics v2. general-
for final fate. `+=` sugar (bench readability, 10 minutes someday).

---

## D17 — defers continue past a failing defer [LOCKED 2026-11-03, patch 39]
Q ruled zig/go-aligned: one failing defer no longer halts the cleanup
chain. the remaining defers still run (the failing thunk's own callees get
their defers run too), and the FIRST failure's error is what propagates -
later failures in the same chain are dropped. the chronology marker (the
thunk frame flag) makes the rule deterministic: an in-flight error that
came from a defer IS the chain's first; an original error is replaceable
by the first failing defer. the defer-owning try still cannot catch its
own defer's failure (the consumed-record ruling, unchanged). oom stays the
stop-everything lane. deferr (error-path-only defer) rides the same lanes.

## D18 — the hierarchy roots are `object` and `type` [LOCKED 2026-11-03, patch 40]
Class -> type, Object -> object, joining the snake_case builtin-class
convention (str/num/bool/array/map/function/nil/coro/object/type). object
is the root every hierarchy funnels into; type is the class of every class
value (the one self-loop). prints follow the names: <class object>,
<class type>. breaking rename, no aliases - pre-1.0 doctrine.

## D19 — modules: the 0.8.0 wave, "O Resolution" [LOCKED on the M1-M14 rulings, 2026-11-03]
the compile-time import model of D9 carries (slot-array exports, run-once
top level, hard-fail cycles) with Q's ruled deltas: QUOTED paths (M1);
path-binding namespaces — `import "a/b"` becomes a.b.XXX, roots merge,
`as` binds the module (M5); natives first-class in the resolution walk at
every step, loadable with no active project, `--force-native` as the last
resort (M3/M6); NO dynamic lane this wave at all (M4); okproject.ok
evaluated in a full-stdlib no-natives instance (M10) with DECLARED build
commands instead of cmake coupling (M12); no package manager, maybe ever —
vendor via git (M11); collisions and cycles both hard errors (M13); the
VM stays out of it — loader vtable + ok_api vtable are the whole boundary
(M14). NEW with the closure: the capability sandbox (OK_CAP_NATIVE_LOAD /
NATIVE_BUILD / HOST, ratchet-only — drop never grant, CLI presets +
embedder ok_specs + the structural loader-vtable denial) lands IN THE SAME
PATCH as the native loader so loading is born gated. the patch series is
re-densed to four (OR-1 spine, OR-2 std re-home, OR-3 sandbox+natives+
embedder loader, OR-4 okproject+build — docs/drafts/MODULES.md). the coro
C API (appendix A: ok_coro_new/resume/status/current +
ok_native_suspend) is specced as patch 42 on the 0.7.0 lane beneath it,
because the ok_api vtable must carry the coro primitives from day one.

## D20 — every ok string is interned; === is content truth [LOCKED 2026-11-03, patch 41]
Q ruled the flip the old C11 note was waiting for: the interner stops
being a compiler-names-only table and becomes THE canonicalizer. every
string - literals, concat results, conversions, class/method names,
host-created values - settles on one canonical object per content, so
`'a' === 'a'` is true (=== stays raw word identity; interning is what
makes the words agree). the table is a WEAK set: mark never walks it,
sweep removes unmarked entries first (okmap-family backward-shift, no
tombstones), so a dead canonical never dangles in a probe chain and an
interned string lives exactly as long as something else holds it. the
single choke point is create_object_string (reaching the interner
through the gc's watch), which is also the answer to "intern all ok
strings without doing it by hand" - zero signature changes, every call
site covered. amends D2's "no string interning anywhere" clause and
D13's "interning stays OFF". the cost: one hash+probe per creation
(hash was already computed for the header) and a rebirth probe when
churned content returns; bounded memory proven (300k churned strings
under a 64mb cap). gates: 427/427 x5 (plain, threaded, asan,
aggressive-gc, asan+aggressive).

## D21 — the coro C API: embedders drive coros, natives park callers [LOCKED 2026-11-03, patch 42]
the "vm_tick" investigation closed: no ready-queue pump exists or should
(scheduler = stdlib = casio, standing veto) - what was missing was the
primitive set the future pump is built from. ok.h grows ok_coro_new /
ok_coro_resume / ok_coro_status / ok_coro_current (thin honest wrappers
over coro_drive with ok_call's C boundary discipline: errors land in
ok_last_error at the host boundary, fail the enclosing call inside a
native; a dead coro answers null per R21) plus ok_value_is_coro and the
OK_CORO_* status enum. the one real piece of machinery is
ok_native_suspend: a native requests suspension AT ITS RETURN BOUNDARY -
a flag on the execution state, checked by the six native call-completion
arms (OP_CALL x4, INVOKE, SUPER_INVOKE), parks the coro exactly like
OP_YIELD would: the native's result goes out to the resumer, the next
resume's argument lands in the call's result register. the yield barrier
doctrine guards it identically (no suspend on main, under an operator /
constructor / unwind defer, or across another native call - request-time
refusals, so the flag never outlives its flight). this is the surface
OR-3's ok_api vtable wraps from day one (D19's ABI argument), and asio's
sleep/await are now writable. gates: e2e 427/427 x5 + unit/embed/capi x5
(tests/capi/ is new: pure C99, 140 checks, doubles as OR-3's
memory-backed-loader bed); delta blue flat (the six flag checks cost
nothing measurable). caught en route: patch 40's rename broke the embed
test's one `Object` - the gates had been quoting e2e only; fixed, and
the unit runner lane is back in the receipts.

same session, Q's review round on the modules draft (recorded here
because D19 is the locked parent): the manifest is **project.ok** (not
okproject.ok - "the file ext gives it"); **coro stays a core native**
like str/num/bool/map/array/type, NOT gated in std - R24's std.coro home
is overruled, OR-2 shrinks to the flat stdlib's std/ re-home; platform
independence by construction (OK_API export macro, dlopen/dlsym behind
the compile-time wrapper); and the host provides dynamic loading
malloc-style (ok_dylib hooks, platform default, hard off-switch) - the
language runs on platforms with no loader at all, and the off-switch is
the structural sandbox denial. v0.7.0 is named **"Epigone"**. OPEN for
Q, Z recommending IN: the ok_api read side, type/class identity
(classof/is_instance/class_name), native->script calls, and the
single-integer OK_NATIVE_ABI versioning stance (MODULES.md section 11a).

## D22 — the diagnostic doctrine [LOCKED 2026-11-03, patch 45]
Q's ruling from the OR-1 landing review, standing for every diagnostic
the language prints from now on. two halves. one: the message carries
the helpful half only - the fix, the expectation, the location - and no
editorial asides (the "python's plain import is not the language" tail
is the example that died: the spelling hint stays, the commentary
goes). internal draft references (M-numbers, OR-numbers) never appear
in user-facing text either: the draft is docs, the message is a lesson.
two: standard punctuation only - comma, colon, period. no dash, no em
dash, no semicolon. semicolons inside quoted syntax examples are code,
not prose, and stay legal. patch 45 swept the whole tree (13 messages:
the six OR-1 offenders plus the seven pre-existing dashes and
semicolons hiding in the parser, compiler, and vm).

same review round, the manifest amendment recorded under D19/M10:
project.ok exports its table with module-level `pub`, not `glob` -
"it teaches bad practice of using glob to export stuff". the manifest
is a plain module; OR-4's evaluator harvests the pub export through
the ordinary module machinery. the sandbox would have kept the global
from leaking anyway, but the manifest models the right shape.

## D23 — the native module ABI [LOCKED 2026-11-03, patch 48]
Q's ruling on the ok_api vtable's scope: "we should support everything
we have and expose to the abi". the read side (value_type/as_number/
as_bool/as_string/array_len/array_get/map_len/map_get), type and class
identity (classof/is_instance/class_name), native-to-script calls, and
the coro primitives including native_suspend are all in; versioning is
a single OK_NATIVE_ABI integer, checked at load, no semver, "rebuild
your natives" is the honest pre-1.0 stance, entries append-only. two
entries the draft missed and the first real module could not live
without: api->view (string-view construction - a native links against
no oklang symbols, so it cannot call ok_create_string_view) and
api->module_export (the mount callback's export primitive: a class or
constant joins the module through it, the namespace freezes when mount
returns). the table is STATIC and stateless, one per process: every
entry routes through its ok* argument, a module stashes the pointer at
export time and it stays valid forever, several vms coexist, no
dangling. deliberately OUT of v1 (recorded so it is a decision, not an
oversight): method enumeration, dynamic member get/set, subclass
creation from native, global define/get (a native's registration is
its module membership, the global namespace stays the host's). same
ruling, same patch: the read side's ok_cstring_view chars are
NUL-terminated vm strings (every object string's storage carries its
terminator), strlen is the native-side length.
