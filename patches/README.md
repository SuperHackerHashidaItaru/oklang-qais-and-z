# patches — the staging area

deliverables awaiting Q's review + hand-application live here, newest last,
stacked in apply order. **when Q applies + pushes, they move to
`archive/patches/` and this dir empties.**

1. `17-map-core-and-globals-store.patch` — vs v0.1.0 (Credence, `9ad153c`).
   the 0.2.0 maps series, patch A of three (plan + rulings:
   `docs/drafts/MAPS.md`). `src/okmap.h`: the new table core —
   MAP_DECLARE/MAP_DEFINE macro (one algorithm, any key/value/probe types),
   open addressing, linear probing, parallel key/hash/value arrays with
   CACHED hashes (stored keys never re-hashed on grow), backward-shift
   deletion (no tombstones), load factor 0.7, power-of-two capacity.
   globals_store migrated to the first instantiation: raw string keys,
   global_id values, string-view probes — **a lookup now allocates
   nothing** (the old code built + freed a hashed_string per add/get);
   hashed_string is dead code after this. `tests/unit/src/oktest_map.c`:
   9 tests / 3 instantiations (real string shape; adversarial two-bucket
   hash; identity hash for wraparound placement), the design doc's worked
   example as a literal slot-layout test, 2000-op property test vs a shadow
   model with periodic invariant checks, 1000-op adversarial property test.
   verification: e2e 211/211 x plain/asan/asan-aggr, embed test x3,
   oktest_map 9/9 x3 (sandbox via a minimal unity shim at
   `scripts/unity_shim/` — not part of the patch; Q's cmake/unity build is
   the real one). honest A/B vs v0.1.0 on a 2000-global compile stress
   file: PARITY (63ms vs 65ms / 20 runs, -O2, outputs identical) — the
   alloc kill is sub-noise at this scale; the win is structural (no
   transient garbage in compile->run handoff windows). notes: GC untouched
   by design (mark_globals_store walks values only); oktable.h STAYS
   (scope tables migrate in patch C, the dead OK_DEBUG debug_table decl
   with it); pre-existing okutils.h hash_combine -Wparentheses warning
   noticed, not touched.

2. `18-compiler-scope-tables-to-okmap.patch` — vs maps-staging (`2239723`,
   Q's applied patch A). the 0.2.0 maps series, **patch C delivered before
   B** (Q's call: finish the internals first). compile-time only, zero
   runtime surface change: `scope_registers` migrates from oktable.h's
   separate chaining to okmap.h — same instantiation shape as
   globals_store's globals_map (owned `string` keys, uint16 register-index
   values, `ok_string_view` probes), `scope.registers_table` renamed
   `registers_map`. the whole `hashed_string` dance dies in the compiler:
   add_local no longer builds an allocated key copy per declaration (the
   probe is the AST-owned identifier view; the map clones the key on the
   insert path only), resolve_local/resolve_upvalue take the view directly,
   and resolve_variable's create/deinit-per-lookup wrapper is gone — along
   with its TODO ("make globals store accept direct hs", now moot). the old
   impl is dead: `oktable.h` deleted, `oktest_table.c` deleted (its
   scenarios are subsumed by oktest_map.c's stronger property tests), the
   never-defined OK_DEBUG `debug_table` decl removed, and `hashed_string`
   itself deleted from okstring.h/.c (the wrapper, exactly as §3 ruled —
   "the thing that dies is the WRAPPER, not the generality"). ~51 ins /
   ~558 del. verification: build x3 + paranoid clean, e2e 211/211 x
   plain/asan/asan-aggr, embed x3, unit x3 (array/map/nan) on the sandbox
   tree AND on a fresh 2239723 + patch-18 apply + rebuild. honest A/B vs
   maps-staging on a purpose-built scope stress (300 functions x 3 nested
   blocks x 30 locals + shadowing + closures + the local->upvalue->global
   miss chain, 62k lines, python-mirrored expected output): outputs
   byte-identical, and unlike patch A's parity-on-globals this workload
   shows a real win — **median 46ms -> 42ms, min 45ms -> 40ms** (20
   interleaved runs, -O2, reproduced twice): the per-probe
   alloc+free-per-miss is gone from the hottest compiler path. notes: the
   13.2 follow-up "cached global ids" targeted the pre-patch-A hashed_string
   lookups which no longer exist; a full startup-alloc survey stays with
   patch B as planned.

next expected: patch B (surface maps: object_string hash field, map object,
literals `{k: v}` / `{:}`, [] strict + `get()` lax, for-in-map opcodes,
ok_map_* API — unchanged by C landing first).

## history

- round 10's arrays-v2 + default-args patches: APPLIED by Q (squash
  `9e9fd41`).
- round 11's numeric-for / disassembler / assign-peephole (01-03): APPLIED
  by Q (squash `55e1293`).
- round 12.5's buffer-night patch (04: ternary, string concat, equality
  fix, token purge, elision, window fix): APPLIED by Q (squash `4633dc9`).
- night-13's 05-modulo + 06-forloop-imm-fastpath: APPLIED by Q (`442ad31`).
- 07-floor-modulo + 08-github-ci: APPLIED, tagged v0.0.5.
- 09-no-question-unbreak + 10-gc-guard-stack-and-okrun-rooting +
  11-as-tests: APPLIED by Q (`52b3055`), merged to master `bc272e3`.
- 12-rooting-and-traced-natives + 13-array-public-api + 14-print-hook +
  15-ci-aggressive-lane: APPLIED by Q (`83dd40c`), later follow-ups
  (debug-mode tests, `%g` number formatting, corpus update) → master
  `9ad153c`, tagged **v0.1.0**.
- 16-tree-sitter-oklang-no-question-update: APPLIED by Q to the
  tree-sitter-oklang repo (grammar + queries + corpus, 131/131).

