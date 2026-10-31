# MAPS — 0.2.0 candidate, first feature draft

status: DRAFT for Q's review. updates and supersedes `docs/specs/maps-v2.md`
(the oct24 spec — its core algorithm, hashing, and what-NOT-to-do sections
STAND; this draft carries them forward and adds what changed since: current
syntax post-no-question, the native API in the patch-12/13 world, for-in
without the multiple-returns coupling, the macro-generality ruling, GC
tracing details, and the patch plan). read with ROADMAP-v1.md §2.

## 1. what ships (surface)

```ok
let m = {"a": 1, "b": 2};         // literal (expression position)
let x = m["a"];                    // read: missing key -> ERROR (R1, ruled by Q)
let y = get(m, "zz");              // the lax path: null when absent (builtin, mirrors len/remove)
m["c"] = 3;                        // write: insert or overwrite
m["a"] += 1;                       // compound: missing key -> error
print(len(m));                     // live pair count
remove(m, "a");                    // mirrors arrays' remove
for k, v in m { print(k, v); }     // iteration (see §4 — no MR coupling)
for k in m { print(k, m[k]); }     // keys-only form compiles too
```

- key types v1: strings + numbers (+bools if free). null key = error.
  arrays/maps as keys = error v1 (identity semantics arrive with classes).
  null VALUES are fine — only keys can't be null.
- mutation model: same as arrays — the binding is immutable, the container
  mutates.
- equality: identity (`==` on two maps = same map), same ruling as arrays.
- `m as map` type check through the existing tag machinery — maps get a tag
  now, which is also the bridge to 0.5.0's proper types.

**R1 RULED by Q — [] errors, get() is the lax path.** my null lean was
lua-shaped and loses on oklang's own stated values: silent-absence is
silent coercion, and the surface owes the programmer CONSISTENCY, not
implementation details — arrays error on [], so maps error on []. python
(KeyError), C++ (map::at), rust (panic) agree; lua's nil-default and JS's
undefined are the famous footguns. the config-shaped "absent = use default"
case is served EXPLICITLY by `get(m, k)` (null when absent; a 3-arg default
form if it itches). compound-assign on a missing key errors naturally via
existing null-arith — same strictness, for free.

**parser note** (real, new since the spec): bodies are brace-delimited now
(`if true { ... }`), so `{` is overloaded. disambiguation is position-based
and standard: **statement-position `{` = block, expression-position `{` =
map literal**. `let m = {}` and `f({"k": 1})` are unambiguous; a bare `{}`
statement = empty block (useless, legal, tested). the fuzz corpus gets
cases; the tree-sitter grammar needs the same disambiguation when this
lands (patch note for Q's tree-sitter repo).

syntax RULED by Q: colon in the literal; `{:}` is the empty map (a bare `{}`
statement stays an empty block / is rejected as a statement — ambiguous,
zero users — while `{:}` keeps the useful empty-map feature alive; bindings
are immutable so `let m = null` then assign-later needs mut, making the
empty literal genuinely load-bearing). oklang is not swift — no lambdas
without `()` — so the `{:` prefix disambiguates cheaply in the parser.
trailing comma: allow. tree-sitter grammar updates in the same series.

## 2. the core — restated from the spec, with the tombstone story taught

current `oktable.h`: separate chaining, a dynamically-grown array per
bucket. pays one allocation per distinct bucket, pointer-chases on every
probe, and puts hash quality of `hashed_string` on the critical path. the
rewrite deletes all of it.

new shape (from specs/maps-v2.md, stands):

```c
typedef struct {
  allocators* alloc;
  uint32_t count;      // live entries
  uint32_t capacity;   // power of two, >= 8
  key_type keys[];     // parallel arrays: probes touch keys only
  value_type values[]; // values stream in after the hit
} map;                  // empty slot = a strictly-null key
```

- probe: `idx = hash(key) & (cap-1)`, linear forward, wrap by mask.
  parallel key array = every probe is one cache line of keys; at lf ≤ 0.7
  average probes stay ~1.5.
- empty = strict-null key. strict tags can't alias doubles (the lesson
  already paid for in okvalue.h) so null keys are unambiguous. this is why
  null KEYS are disallowed — the encoding spends them on "empty".
- load 0.7, grow ×2 after insert, optional shrink below 0.15 (games leak
  maps otherwise).

**deletion: backward shift, no tombstones** — Q knows the tombstone way, so
here is the whole idea honestly. tombstones mark deleted slots as "keep
probing past me"; they work, but they're a third slot state that accumulates
— chains lengthen, load-factor accounting gets muddy (do tombstones count?),
and eventually you rehash-to-clear a table whose live count never grew.
backward-shift deletion exploits the thing linear probing gives you for
free: **entries live in contiguous clusters starting at their home slot.**
to delete slot i: walk forward through the cluster; every entry whose home
is at-or-before the current hole could legally live in the hole, so slide
it back; the hole moves forward; when you hit an empty slot, done. every
entry stays within probe distance of its home forever. no third state, no
counters, no rehash-on-delete. (this is exactly why quadratic/double
hashing NEED tombstones — their entries don't form contiguous clusters, so
gaps can't be healed. the algorithm choice and the deletion choice are one
decision.)

worked example, 8 slots:

```
insert A (h=0): [A _ _ _ _ _ _ _]      A@0
insert B (h=0): [A B _ _ _ _ _ _]      B probes 0, lands 1
insert C (h=0): [A B C _ _ _ _ _]      C probes 0,1, lands 2

delete B:
  hole = 1
  look at C@2: C's home is 0, which is at-or-before the hole -> C slides
  into 1, hole becomes 2
  slot 3 is empty -> done
  result: [A C _ _ _ _ _ _]

find(C) now: home 0 -> A ≠ C -> slot 1 -> C. correct.
had we just nulled B without shifting: [A _ C ...] -> find(C) stops at
the empty slot 1 and returns MISSING. that lost-entry bug is what
tombstones prevent the dumb way; the shift prevents it by construction.
```

the wrap case (h=7 wrapping to 0,1) is the same comparison done cyclically;
the patch carries the exact C form (`entry at k may fill hole j iff
((k - home) & mask) >= ((k - j) & mask)`) — the doc carries the intuition.

## 3. one core, many users (the stdc++ question, answered)

Q asked: general for internals and surface, "maybe like stdc++ just
separate chain?" two readings, both answered:

- **separate chaining** (the collision method): no. std::unordered_map
  chose chaining for iterator-stability rules that don't bind us, and it
  pointer-chases — it's what we're deleting. linear probing is the whole
  point (cache lines + backward-shift deletes).
- **separate implementations** (libc++'s private internal containers): no
  for the table's ALGORITHM — but the generality split is now FACT-BASED,
  not taste (ruling below): the surface map object is value-native (ok_value
  keys AND values, strict-null key = the empty slot — Q's instinct, adopted:
  it deletes the hashed_string wrapper entirely); the internal name index
  and the future string-interning table instantiate the SAME algorithm at
  raw-string key type, because their keys are plain allocator strings and
  their probes must not allocate. one algorithm, two macro flavors, zero
  wrapper types. the thing that dies is the WRAPPER (hashed_string), not
  the generality.

## 4. iteration — and why maps do NOT depend on multiple returns

the spec sketched `for let k, let v in map ?` next to a MR-era mindset. we
don't need it: the ARRAY for-in is already a dedicated compiled path in the
VM, not a generic iterator protocol. the map gets its sibling: a dedicated
for-in-map opcode pair that walks occupied slots and writes **two loop
registers** (k, v) per step. no span machinery, no allocation per step, no
coupling to 0.3.0 — maps can be 0.2.0 complete. (lua needed multiple
returns for `pairs()` because its for-in is a generic protocol; ours is
compiled special-cases. keeping that is the design move.) if a generic
protocol ever arrives, it generalizes over these.

iteration order: **unspecified v1** (it's slot order — an implementation
detail), documented as such. python-style insertion order is a known
upgrade (entries vector + key→index) — post-1.0 itch, R3; content work will
ask for it eventually, and that's fine.

## 5. hashing

- numbers: splitmix64 over the raw double bits, **-0.0 normalized to +0.0
  first** (R2 — otherwise 0.0 and -0.0 are "different keys" that compare
  equal in some paths and not others; normalize and document).
- strings: FNV-1a over bytes, **cached in the string header at creation** —
  immutable strings, computed once, forever. today the hash lives in the
  `hashed_string` wrapper used by internal tables; the surface map wants it
  in `object_string` so map keys don't need wrapping. small rework, lands
  here (patch C).
- every other object type (post-classes): pointer → splitmix64 (identity
  semantics match `==`).
- **mix the TAG into the final hash** (`h = splitmix64(h ^ tag_bits)`) so
  `1`, `true`, `"1"` can never share a bucket path. equality must agree
  with hashing or the map silently lies.
- key equality: strings = content equality (cached-hash fast reject, byte
  compare on hit); numbers = bit equality post-normalization. no interning
  required — interning is a separate later perf option (pointer-eq keys),
  explicitly NOT a dependency of this feature.

## 6. GC + native API (the patch-12/13 world)

- surface map = new object type, sibling of okarray: header + the two
  parallel arrays. the tracer marks **every live slot's key AND value** —
  both are GC refs (strings are heap objects). same lesson the old spec
  already flagged for arrays.
- rehash = allocate new arrays, move, old arrays become garbage. happens
  only in write paths (insert/store), never during trace — the
  no-alloc-during-trace contract from patch 12 is preserved by
  construction. `map_get` is read-only and allocation-free.
- native API mirrors patch 13 exactly: `ok_map_new / ok_map_get /
  ok_map_set / ok_map_remove / ok_map_len` + `ok_value_is_map`. the
  constructor guard pattern carries over: a host-built map stays rooted
  across the whole fill loop (each set may rehash under aggressive-gc).
- embed test: host builds a 300-entry map under asan-aggr (forced rehash
  mid-construction, collections everywhere), script reads it; script
  builds, host reads + mutates. the string_bag shape, table edition.

## 7. implementation plan (patches, the usual rhythm)

**patch A — DELIVERED (session 19): `17-map-core-and-globals-store.patch`**
vs v0.1.0 (Credence). `src/okmap.h` (the core: MAP_DECLARE/MAP_DEFINE,
linear probing, parallel key/hash/value arrays, cached hashes — stored keys
are NEVER re-hashed on grow — backward-shift delete, lf 0.7, power-of-two
capacity) + globals_store migrated to the first instantiation (raw string
keys, global_id values, **string-view probes — a lookup allocates nothing,
the old code built and freed a hashed_string per call**) + hashed_string
deleted from its last user + `tests/unit/src/oktest_map.c` (9 tests, three
instantiations: the real string shape, an adversarial two-bucket hash, an
identity hash for wraparound placement; the doc's worked example as a
literal test; 2000-op property test vs a shadow model with invariant
checks; 1000-op adversarial property test with final slot-invariant walk)
+ test_map added to the unit CMakeLists. gate: e2e 211/211 x
plain/asan/asan-aggr, embed x3, oktest_map 9/9 x3 (sandbox ran it via a
minimal unity shim — Q's cmake/unity build is the real one). honest A/B:
compile-heavy micro-bench (2000 globals + 2000 refs) shows PARITY with
v0.1.0 at -O2 (63ms vs 65ms / 20 runs, outputs identical) — the transient-
alloc kill is real but sub-noise at this scale; its value is structural
(no transient garbage in the compile->run handoff window — the 13.2 bug
class shrinks). the GC is untouched by design: mark_globals_store walks the
values array only, never the table internals (verified before writing).

**patch B — the surface**: object_string gains a cached `hash` field
(computed at creation — needed by value-keyed string lookups; this was
formerly patch C's first half, moved here) + the map OBJECT (ok_value keys
and values, strict-null key = empty slot) + literal `{k: v}` / empty `{:}` +
index r/w ([] missing = error per R1; write = insert-or-overwrite) + `get()`
builtin (the lax path) + len/remove + the for-in-map opcode pair + errors +
tag + tracer (marks key AND value arrays) + native API ok_map_* mirroring
patch 13 + e2e files (literals, missing-errors, get()-null,
compound-on-missing, iteration, mutation, key-type errors, null-value
storage, aliasing, literal-in-call, the {:} parser cases) + asan-aggr
torture (rehash mid-fill under collection) + tree-sitter grammar delta
(separate repo patch, same series).

**patch C — the cleanup**: compiler scope tables migrate to okmap.h +
oktable.h deleted + the dead OK_DEBUG debug_table declaration removed +
(here or in B) any startup-alloc leftovers. compile-time-only, zero runtime
risk, pure churn — kept separate so patch A stays reviewable.

**bench trio** (lands with B): insert-heavy, lookup-heavy, churn-heavy, vs
lua tables and python dicts, on the real bench protocol (the micro-bench
above is a sanity check, not a bench).

## 8. rulings — ALL CLOSED (session 19)

- R1 missing key via [] = ERROR; `get()` builtin is the lax null path — Q
  ruled, Z conceded (§1)
- R2 -0.0 key normalization: normalize + document — Q leaned with Z
- R3 iteration order: unspecified v1, documented as such
- literal syntax: colon; trailing comma allowed; empty map = `{:}`; bare
  `{}` statement rejected
- Q6 (value-native table): ADOPTED for the surface map object; the name
  index + interning stay at raw-string keys through the same macro — the
  deciding fact is that `string`/`hashed_string` are plain allocator
  buffers, NOT GC objects, so making globals_store keys into values would
  push the compiler's identifier churn through the GC and widen the exact
  13.2 handoff window we just closed. one algorithm, two flavors, zero
  wrappers (§3)
- backward-shift deletion: Q read up on it independently and validated —
  both known weak points don't bind (O(k) delete is expected-slow;
  single-threaded language, no concurrent-shift concern)
