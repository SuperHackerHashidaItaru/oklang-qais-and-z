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


## 19 — map-surface (MAPS.md patch B, the last maps patch)

vs maps-staging `0987b4e`. the surface: literals `{k: v}` / `{:}`, polymorphic
index r/w (OP_ARRAY_* renamed OP_INDEX_*), the ITER trio (for-in over arrays
AND maps, one- or two-binding; mid-iteration size change = hard error),
get/len/remove builtins (variadic-native sentinel), `as map` tag, tracer,
native API ok_map_*, 50 e2e files, embed map section. includes the string
-hash fix (standard FNV-1a — okutils' variant's low bits collapse on short
ascii keys) and three flagged drive-by fixes (array_as_str separator +
leak, debug-print tags). full what/why/blast-radius: 19-map-surface.md (also
in download/). READ THE .md FIRST — it carries the one syntax call
(`for let k, v : m`) and three honest notes needing Q's eye.


## 20 — as-map repair (the v0.2.0 hole)

vs master `127ff0b` (v0.2.0). the release is missing the `as map` surface Q's
maps-staging has: the TT_MAP case in perform_as, the `" map"` hidden type
intrinsic in ok.c (three spots), and as_map.ok — the enum line
(`temporary_type_tags` + TT_MAP) is what the hand-apply missed, which is
why staging didn't compile and why the as-map hunks presumably got dropped
from the release assembly. this patch is exactly those four pieces; applying
it to master completes Etymos. (staging already carries the content — it only
needs the one-line enum fix, which this patch also brings.)

## 21 — spans patch A: multi-return + destructuring (SPANS.md)

vs staging `c75c583` + the TT_MAP enum line (i.e. master + patch 20). the
surface: `return a, b;` (every path agrees, compile error with both lines on
mismatch — R2 inference), `let (x, y) = f();` (per-binder mut, R1 parens),
proto `results` field, call() strictness (R6: "it returns 2 values, 1 were
requested here" — runtime, see notes), ok_call first-of-span (§6). 26 e2e +
embed spans section. bytecode-identical for every single-result caller that
takes 1 (verified by full paranoid trace diff). HONEST NOTES in the worklog
and SPANS.md §7: fall-off-the-end of a span function null-pads (null, null);
`return f();` is not span forwarding (destructure first); the arity check is
runtime because callee protos aren't statically reachable through globals
yet — the interning-era known-globals table would make it compile-time.

## 22 — spans patch B: varargs + spread (SPANS.md)

on top of 21. the surface: `fu f(a, rest...)` (R3 token), VA_PACK prologue op
(one array allocation per call, always-pack v1), call() floor drops the
varargs param / no ceiling / frame widens past `registers` to cover the
staged trailing args, `g(args...)` spread via CALL_SPREAD (+_LONG) that
stages the elements into the window at runtime (R5: SHIPS). defaults +
varargs compose (`fu f(a, b = 2, c...)`, the ladder range excludes the
varargs param). 19 e2e (sum_all shapes, empty, defaults, capture, spread of
empty/computed/mixed, gc-churn torture, arity/type/position errors) + embed
varargs section + bench trio (mr_call, varargs_pack). spread in array/map
literals and non-last call positions: precise compile errors.

## 23-27 — the errors series (ERRORS.md, ruled E1-E8): diagnostics + throw/catch/finalize + source ownership + the C boundary

vs master `0193e08` (spans). five patches, apply in order. the phase in one
line: patch 23-25 rebuild what you SEE when something fails (okdiag
renderer, true locations, source ownership), 26-27 are the model (errors as
values, handler frames, finalize, the host API). rulings E1-E8 folded in
`docs/drafts/ERRORS.md` §9, the honest deltas from the draft in §11 —
read §11 before reviewing, three deviations are load-bearing.

**23 — diag core.** `src/okdiag.c/h`: the one renderer (gcc-parseable
`path:line:col:` headers + rustc-style snippets, notes with their own
spans), spans-from-pointers at token creation (`ok_line_info.offset` is now
a true file-absolute byte offset; the accumulated counter is dead — R1),
error tokens render one caret on the offending byte instead of printing the
message as the "line" (R2/R6), the phantom empty note is gone (R3), EOF
errors render the last line + `(at end of file)` (R5), the RLE line table
gets a debug assert. bonus fix: the parser's `asprint("%*.s")` typo (empty
error messages, hidden for months). 6 snippet-contract e2e.

**24 — runtime locations + notes.** R4: the dispatch loop owns the live ip
— `runtime_error` now only stashes (no print/reset/alloc) and the failing
arm's `FAIL_PATH` reports from the live state, so BIN_OP/INDEX errors carry
locations for the first time ever. measured first: a per-instruction store
cost +10-18% on numeric_for_sum and was declined — this is lua's
savestate/Protect shape, and it pulled the error lane (planned for 26)
into this patch. binder receipts in the scopes: redefinition and
immutability notes get real secondary snippets (`note: previous declaration
of 'x' is here.`, `note: declared here without 'mut'.`), locals AND
upvalues (the upvalue note points into the declaring function). R10 sweep
(pluralization, `got:` colons, trailing periods, "convertable", trace
headers). 5 e2e.

**25 — source ownership (E8 as ruled: opt-in).** the vm owns a per-run
source table; chunks carry source ids (stamped over the compiled tree,
nested protos included); reports read per-frame — an A-compiled function
called during run B reports A's path with A's line:col, excerpt only while
A's text is alive (borrow expired → header-only: degraded, never wrong —
R8 dead). `ok_retain_source(ok*, bool)` opts into ok-owned copies until
ok_free (the REPL rides it; the CLI borrows; future okc pays nothing).
`ok_source.from` + `OK_FROM_*` DELETED (E6: write-only since birth).
`ok_line_info` gained a stored `column` (lexer-computed: true line:col even
without source text). ok.h's ownership comment tells the truth now (R7).
embed: A/B borrowed + retained (stderr captured, asserted), stack-buffer
reuse.

**26 — throw/catch.** THREE opcodes: TRY_ENTER (2w: catch offset, park
register, upvalue floor) / TRY_EXIT / THROW. no CATCH_BIND — the record
parks the value straight into the binder's register (zero MOVEs, the spans
window trick). handler records on ONE vm-wide frame-tagged stack (try-free
scripts pay nothing); frame pops drop their records; break/continue emit
TRY_EXITs for the regions they cross (a stale record hijacking a later
error was the correctness trap). the unwind is a loop (D7): pop frames
above the handler (close their upvalues), close above the floor, restore
the watermark, materialize + park, resume. the lane: script `throw v`
carries the value whole (GC-rooted while pending); runtime errors carry
kind + text and materialize `{:kind:, :what:}` only when consumed (E2, your
field names); OOM is a hard lane (runtime_oom: not a value, not catchable).
kinds swept across all ~75 raise sites (type/arity/index/key/value/
conversion/native/internal). two perf bugs found + fixed on the way (see
§11). 11 e2e + the cross-native embed section (throw through ok_call into
a native that ignores the failure — the catch still takes it).

**27 — finalize + C API.** finalize with NO new opcode: LOAD_TRUE/FALSE
stage the entry path, a plain JUMP_IF re-dispatches after F, and the record
covering F reuses the park register — an error in F replaces the in-flight
error by construction (E4 as ruled: runs on normal + unwind, before catch).
`try {} finalize {}` (no catch) re-raises outward. the C boundary (E7):
`ok_last_error` (your thrown value as-is, or a fresh {:kind:,:what:} map —
unrooted, root it or allocate before fetching), `ok_clear_error`,
`ok_native_throw` (the host's value crosses into script catch as itself;
ok_native_error stays as message sugar). host-caught ok_call failures print
NOTHING — the uncaught-to-top report stays ok_run's. 6 finalize e2e + embed
sections (rooting contract demonstrated with ok_root, the E7 silent lane,
native_throw, the ok_run report).

gates: e2e 333x4 (305 at master, +28), unit x3, embed x3, spans corpus
disassembles identically (verified by diff). v1 limitations deliberate and
listed in §11 (break/continue/return skip finalize; REPL per-line
numbering; rethrow's report span). honest note: my first cut of the
ok_last_error embed test violated the API's own rooting contract under
aggressive GC — the test now demonstrates the contract instead of dodging
it, which is the review-worthy part.
