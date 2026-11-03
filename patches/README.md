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

**27 — the C boundary (v2: finalize is dead).** Q killed finalize before
it ever shipped ("kill finally, embrace defer — why have 2 ways if one do
trick"), so this patch is the C API alone: `ok_last_error` (your thrown
value as-is, or a fresh {:kind:,:what:} map — unrooted, root it or
allocate before fetching), `ok_clear_error`, `ok_native_throw` (the host's
value crosses into script catch as itself; ok_native_error stays as message
sugar). host-caught ok_call failures print NOTHING — the uncaught-to-top
report stays ok_run's (E7). embed sections: the rooting contract
demonstrated with ok_root, the E7 silent lane, native_throw, the ok_run
report lane.

**28 — defer + deferr.** the replacement, zig style: per-scope scheduling,
one mechanism for both spellings. `defer f(args)` snapshots the callee and
arguments into hidden locals AT THE DEFER SITE (Go semantics — later
reassignment changes nothing), and the thing that runs is a parser-built
thunk (`fu() { dc(da0, ...) }` over the hidden names) compiled by the
ordinary lambda path — defaults, closures, multi-result callees all work
for free. the schedule itself is the LAST NaN-box tag (TAG_DEFER, payload
= the thunk closure, only ever lives in the hidden slot, GC traces it).
normal exits are COMPILE-TIME visible: scope end / break / continue /
return / fall-off emit OP_DEFER_RUN (LIFO) + LOAD_NULL kills for deferr.
unwind exits are the runtime's: the unwinder scans register ranges (each
dying frame, then the handler region above the record's floor), running
tagged slots descending — thunks stage at the register-file top under a gc
guard, with the error lane parked in C so deferred NATIVES don't trip
call_native's pending-error check (found via `defer len(...)` + throw).
deferr registers identically but is never RUN-emitted: only the unwind
scan executes it (errdefer). cascade: an error that passed through a defer
invocation stops the chain (remaining defers nilled), and the defer's
error replaces the in-flight one — the catch whose region was unwinding
never sees it, handlers further out can. uncaught: the final reset sweeps
every frame's defers before tearing down. THREE real bugs found and fixed
on the way, all aggressive-GC/ASan finds: (1) the main chunk's user scope
ended AFTER the fall-off return — top-level defers (and upvalue closes)
were dead code; (2) the staging slot parked the thunk as a plain value at
`top`, which can sit BELOW an outer frame's compile-time window — the
swept closure gets resurrected as a dangling mark when the outer window
re-covers it (staging writes NULL now, the guard + the pushed frame root
the closure); (3) the scan range could extend above the throw-point's
watermark — unrooted mid-scan slots (the scan now roots its whole range).
14 e2e + an embed section (the uncaught sweep through ok_run with print
routing). bytecode note: the spans corpus is byte-identical EXCEPT
top-level CLOSE_UPVALUES now precedes the fall-off RETURN (was dead code,
now live — that's fix (1), visible and intended).

gates: e2e 342x4 (328 at patch 27, +14), unit x3, embed x3, fresh-apply
of the whole series 23-28 on clean master verified (sequential apply,
pedantic-clean build, 342/342). v1 limitations, deliberate: no spread in
a defer call (clean compile error, wrap it in a function that takes the
array); defer errors are catchable by handlers further out — including
same-frame ones that lexically enclose a NORMAL-exit defer (the ruled
exception: the catch whose region was unwinding never sees one); a defer
failing during the uncaught sweep is dropped after the report (nothing
left to consume it).

## 29 — tree-sitter: defer/deferr in, finalize out (the errors catch-up)

vs tree-sitter-oklang master `72b061f` (your v0.2.0). the grammar had
try/catch/throw but still carried `finalize` and knew nothing of defer —
this is the patch the guide's §10 claimed shipped last session and didn't
(owned in the worklog). the delta: `defer_stmt` / `deferr_stmt` rules
(`'defer' expr ';'`, same for deferr) + both added to `_stmt`; `finalize`
deleted from `try_stmt`; `finalize` deleted from the highlights query,
`defer`/`deferr` added to @exception; the `[($.try_stmt)]` conflict entry
removed (tree-sitter flags it unnecessary post-change); corpus: the "try:
binding + finalize" test rewritten finalize-free, two new tests (defer
schedules a call for scope exit, deferr error-path-only). parser.c /
grammar.json / node-types.json regenerated with tree-sitter-cli 0.27.1 —
note parser.c churns wholesale across CLI versions, the semantic delta is
the grammar + queries + corpus (~60 lines). verification: corpus 132/132
(was 130 + the rewritten pair), zero warnings, and the real e2e sources
from your `errors` branch (defer_basic / deferr_only_on_error /
try_catch_nested) parse with zero ERROR nodes. apply: `git apply` from the
tree-sitter repo root, or read the grammar.js + corpus hunks and type them
(the generated files you regenerate yourself either way).

## 30 — classes: core + construction + statics (Citadel, round 1)

vs oklang master `0666651` (v0.4.0 Eyrie). the first code round of the
classes design, implementing the C1-C18 rulings: `class Name : Parent`
(the `inherits` token is dead, `pub` and `static` are the new keywords);
OBJ_CLASS/OBJ_INSTANCE/OBJ_BOUND_METHOD in the 24-bit info field (no new
NaN-box tags); fields as `let`/`let mut` with initializer thunks run
root-first at construction; `fu init` + `super(...)` with the definition-
time forgot-super check (the compile-time half is a calls-super flag on
the proto, the chain check fires at OP_DEFINE_METHOD - the one place the
chain is knowable); construction is the static `operator ()` (no `new`,
C4): `ClassName(args)` calls a user override or defaults to allocate +
field inits + nearest init; statics (`static fu`/`static let`, pub
composes) in their own table with chain-shared storage (never copied);
privacy per C5/C6 - fields and statics are hierarchy-internal, pub
survives overrides, the signature contract governs virtual methods but
NOT init (a constructor may widen its parameters); sealed tables frozen
at OP_CLASS_END with the in-place name-to-slot freeze pass; INVOKE /
GET_MEMBER / SUPER_INVOKE with per-site monomorphic caches (keyed on the
receiver's class for instances, the class value itself for class
receivers, the lane bit keeps them apart); bound methods (`let m =
obj.method`, C18's question - the ruling is in the round-1 guide);
`this` immutable at r0, capturable in lambdas (bare field names inside
lambdas take the guarded member lane); classes are values (stored,
passed, `==` identity); Object + Class singletons, seeded as immutable
top-level bindings; the disassembler learned every new op (the mid-work
had them missing from OPCODE_NAMES entirely - out of bounds reads in
--dump). 22 new e2e tests in tests/e2e/tests/classes/. gates: e2e 364
x4 (plain/asan/asan-aggr/aggressive), unit x3, embed x3. note: hand-
formatted to the codebase style - no clang-format in this sandbox, run
yours if you want the canonical pass. apply: `git apply` from the oklang
root, or the usual hand-typing. the full walkthrough is
docs/drafts/CITADEL-R1.md.

## 31 — performs: numbers-first + tag switch (C14)

vs the same master, applies after 30. the micro-optimization you asked
for in C14: every binary perform_* (add subtract multiply divide modulo
equal not_equal greater less greater_equal less_equal) rewritten from
the if-chain to your perform_as shape - one numeric check first
(`value_is_numeric`, the strict-int doctrine: raw doubles alias the int
tag, the tag check alone is not a discriminator - the first cut of this
patch shipped that bug and the modulo e2e caught it), int/int stays int,
any float widens through `value_as_f64`, then a `switch (VALUE_TAG(lhs))`
for the object lanes (string concat for +, content compare for ==/!=,
identity fallback for mixed equality preserved exactly). the dead
`IS_VALUE_OBJECT(rhs)` TODO arms are gone - C14 answered them (strictly
lhs, invalid). the four ordered comparisons share one macro. micro-
bench baseline post-rewrite, release build, this sandbox: 5M-iteration
float/int add loops at ~124ms each - the hyperfine A/B against the
0.4.0 binary is one run-benchmarks.sh away whenever you want the table.
gates: same 364 x4 + unit + embed (run after this patch, everything
green together with 30).

## 32 — operators + the bare-name ladder (Citadel, round 2)

vs the 30+31 state, applies after 31. R1 as ruled: methods ride bare-name
resolution - the freeze bakes field -> GET_METHOD (binds this) -> global
for unresolved names in member bodies, stores bake the not-assignable
trap for method names; statics mirror the ladder at runtime in the
GET/STORE_STATIC_NAME miss path (which also fixed bare sibling static
calls and bare glob access from static contexts). the operator surface:
+ - * / % == != < <= > >= unary- [] []= and the instance operator (),
left-dispatch only (C14), != desugars to == plus negation, the ordered
four return the method's raw value. canonical spellings from ONE shared
parser helper ("operator +", whitespace normalized both sites);
non-dispatchable spellings refused with reasons; `operator` reserved as
a field name. one name one owner within a body (the unary/binary minus
split rides the signature contract). the cache is class-resident -
op_cache[CLASS_OP_COUNT] per (class, operator), sound forever under the
C15 seal, zero instruction words. two shipped-bug fixes found by probing:
a class in a method hard-errored at the enclosing freeze (protos now
carry is_class_member; the walk stops at member boundaries), and the
statics-lane miss path had no global fallback. gates: e2e 379 x4 + unit
+ embed. the full walkthrough is docs/drafts/CITADEL-R2.md.

## 33 — conversions + identity (Citadel, round 3, first half)

vs the 32 state, applies after 32. === as word identity (1 === 1.0 is
true - there is no public int box; 0.0 === -0.0 false is the visible
edge); `is` as the exact-class probe (classof+identity fused; the chain
walk is isinstance's job); isinstance/classof as plain natives; operator
<Target> rides the as lane (inherited, private = hierarchy-internal,
result checked); the seven builtin singletons under Object through ONE
seeding table; TAG_AS + TT_ + the hidden " str" globals dead, tag id 6
reserved, the unit test pins the numbering. the round's receipt: the
guard-stack overflow at startup (capacity 16, the sequence was at 17
pre-p33 and hit 24 with the builtins - the str singleton's name was
swept mid-startup and its memory reused by the num name, so `x as str`
probed "operator num"; flaky across suite-vs-solo). fix: per-object
balanced guards, every push checked. also: the asprint leak in every
object_as_str arm, as_num's missing int return + copy-paste message, and
a confessed test bug (operators_subscript.ok mixed expect lanes, hiding
a wrong 40 vs 45). gates: e2e 390 x4 + unit + embed. the full walkthrough
is docs/drafts/CITADEL-R3.md.

## 34 — natives as classes (Citadel, round 3, second half)

vs the 33 state, applies after 33. run_native (the shared flight; closes
a pre-existing 256+-arg truncation+overrun on ANY-arity natives) +
call_native_method (receiver rides argv[0]; the INVOKE / bound-method /
operator-dispatch windows were already the same shape). all seven
'native methods arrive with the native-class patch' traps are real lanes,
including a C-defined init. builtin receivers dispatch on their singleton
through classof, mic-cached. surfaces per the no-new-stdlib ruling: array
len/push/pop/remove, map len/remove/get - the SAME receiver-first entries
mounted twice, so len(a) and a.len() are one function. C API:
ok_class_new / ok_class_define_native_method / ok_class_freeze /
ok_value_is_class, with the script lane's definition rules enforced
C-side. the embed test grows a Math class. gates: e2e 394 x4 + unit +
embed. the full walkthrough is docs/drafts/CITADEL-R3.md.

## 35 — copy-on-redefine (Citadel, round 4, R16)

vs the 34 state, applies after 34. a re-entered local class definition
hands out a fresh class with its own copy of the body: freeze_patch_closure
sees a proto already patched by another class, checks the parent, deep-
copies the proto tree (code, line table, identifiers, upvalue
descriptors, scalars; constants shared except nested non-member protos,
which copy; member protos of inner classes stay shared), rebinds the
closure, and freezes the copy. same parent = identical flattened layout =
the inherited bake is valid; a different parent keeps an honest refusal
(the bake would need pre-freeze code the proto no longer carries). the
rooting discipline: children are created, appended to a safe parent's
constants, THEN filled - the guard stack never holds more than the root
plus one child, regardless of lambda nesting; a failed copy's partial
tree is unreachable garbage the collector reclaims. mic slots copy fresh
(each generation's sites start cold). the R2 error test flips positive;
five new pins including the different-parent refusal and the embed
factory called twice through ok_call. also in the round (not this patch):
bare_builtin_shadow.ok pins a pre-existing finding - a bare call inside
a method reaches the std-zero builtin before the class's own method of
the same name (R17 in CITADEL-R4). gates: e2e 399 x4 + unit + embed.
the full walkthrough is docs/drafts/CITADEL-R4.md.

### 36 — the lightweight perf pass (interning, poly-MIC, fused conditions, threaded dispatch)

four rulings from ROADMAP-v1's PERF pass 1, landed as one patch on top of
classes-staging. (1) threaded dispatch: computed gotos behind
OK_THREADED_DISPATCH, one handler body shared by both modes (CASE/DISPATCH
macros; the switch build compiles identically to before), cmake GNU
auto-detect + OK_PURE as the portability gate, OP__COUNT sizes the table,
the four pre-patch NAME ops alias NOP to match the switch's silent skip.
(2) fused conditions: OP_JUMP_IF_CMP (2w, 3-bit cmp lane) - a direct
comparison in an if/while/for condition is one instruction instead of
two, the perform family reused verbatim (operators still dispatch), the
lhs operand saved/restored so a local's register can serve directly.
(3) the poly-MIC: the per-site cache is a 3-way probe chain with
round-robin eviction; class_poly_dispatch stops thrashing; fresh
generations start cold (R18 carried). (4) interning: an open-addressed
set canonicalizes compiler-made NAMES (member/class/method spellings +
the vm's startup names) so probes hit a pointer fast path - literals stay
uninterned because === is word identity and flipping `"a" === "a"` to
true is a contract change (R19 open). four new e2e tests; gates: e2e
403/403 on BOTH dispatch modes + unit/embed x5 (gcc, asan, asan-aggr,
threaded-asan, threaded-asan-aggr - the threaded sanitizer lanes exist
because the switch build passing is NOT evidence for the threaded build:
the transform-scanner bug hid five wrong break->DISPATCH conversions
behind a green switch suite). the before/after numbers and the receipts:
docs/drafts/PERF-PASS-1.md. the classes gap narrowed (field 1.2->1.14x,
chain 1.4->1.30x, poly 1.3->1.23x vs wren) and the core FLIPPED to ahead
(loop_counter 1.17x, branch_toggle 1.18x, arithmetic_sum 1.24x,
array_index 1.26x); what remains is allocation-shaped - 0.9.0's lane.
also delivered: docs/drafts/COROS.md, the coroutine draft (R20-R24).

### 37 — glob classes published null (the bug Q caught)

`glob class cc { ... }` then `cc().pr("hi")` errored "value is not
callable" and `print cc` said null, while locals worked - Q's report,
root-caused in one read. compile_class_declaration ended with
define_variable(varloc, value-at-varloc): for a LOCAL that shape is the
register no-op (klass_reg IS the local), but for a GLOB the value location
was the global slot itself, so write_global materialized it (LOAD_GLOBAL
null) and stored it back - publishing null over the slot while the real
class rode klass_reg untouched. the define now carries the class in
klass_reg, the exact shape compile_function_declaration always used for
glob fus; the alias case (`glob let cg = cc`) only ever worked when the
register index happened to survive. also splits the committed-broken
operator_errors.ok (three expect-error lanes in one file, but the vm halts
at the first error and the runner checks the last message - it could never
pass) into the three one-error-per-file tests the house convention wants,
and adds glob_class.ok pinning the regression: direct instantiation,
print, the glob-let alias, use from a function, a local subclass of a glob
class, classof identity. gates: e2e 406x406 threaded + pure switch,
classes family asan + aggressive-gc clean.

### 38 — coros: the coroutine lane (R20-R24)

the draft's pitch held: a coro is an object that owns an execution_state,
and resume is a state swap plus a plain run_until(0). surface per the
rulings: `coro` is the eighth builtin class (lowercase, always loaded now,
future home std.coro), constructed through its native static
`operator ()` - `coro(fu)` makes the coro, does not run it. `c.resume([v])`
drives (first resume's argument is the body's argument; a suspended
resume's argument lands in the parked yield expression - the two-way
channel; returns the yielded value or the body's final value; dead answers
null per R21). `c.status()` spells ready/suspended/running/dead. `yield`
is a keyword (R23), statement and expression, bare = null.

the mechanics that mattered: OP_YIELD parks the top frame in place (no
pops, no upvalue closes, no defers) and returns RUNTIME_SUSPENDED straight
out of the driver's run_until - sound because the yield barrier keeps
every C-driven region (operator dispatch, construction, the defer unwind
scan, native ok_call hops) yield-free by error, lua's C-call-boundary
discipline, so nothing between a legal yield and the driver holds state.
the surprise: the NORMAL scope-exit defer lane stages its thunks in the
dispatch loop, so a defer containing a yield parks mid-cleanup and resumes
honestly - generators-in-defers are real, only the error/unwind scan
refuses. error death: the R22 sweep runs the body's defers on the coro's
own state (the uncaught-at-top-level discipline), then the whole error
lane transplants to the resumer - the resume call site is where a catch
takes it. death frees the state; a dead coro costs its object.

upvalue ownership (the real work the draft missed): an open upvalue's
location indexes its OWNER's register array, never the running state's.
main-state captures resolve through vm->main_state; coro captures carry
the coro as a TRACED owner value on the upvalue, so a closure that
escaped a parked coro pins the coro (and its register file) until the
cell closes - the sweep cannot dangle it, close clears the pin.
steady-state read/write loops measure flat; the capture path pays one
word (churn micro ~10%). the gc grows mark_execution_state (one walk for
the current state, the vm->resume_parked resumer chain, and every
reachable coro's owned state).

en route, two pre-existing bugs: the glob-classes null publish (patch 37,
Q's report) and parse_expression's infix loop handing a NULL left to the
next infix after any parse failure - a call node with a null callable
that segfaulted the teardown (any parse error inside an infix chain
could crash the cli; found while writing the coro tests).

gates: e2e 423x423 (was 406) on threaded dispatch, pure switch, asan,
and aggressive-gc. 17 new tests: the channel, bare yield, yield-from-
nested-calls, coro-resumed-from-inside-a-coro, error crossing + R22 +
runtime-kind errors, parked-vs-death defers, defer-containing-yield,
upvalue ownership both directions (shared cells + the escaped-closure
pin), gc churn between resumes, the running status window, classes
interplay (instances across parks, bound methods, construction inside),
and the eight error lanes (yield outside/constructor/operator/defer,
resume running, resume arity, body-not-a-fu, body arity).

### 39 — defers: continuation on failure (zig/go doctrine)

Q's ruling on the defer question: a failing defer must not halt the
chain. run_pending_defers no longer nils the rest of the range - the
remaining defers run, the failing thunk's own callees are torn down with
THEIR defers run (cleanup on the way out, whatever failed), and the
FIRST failure's error is what propagates. the chronology marker: the
thunk frame flag (formerly the cascade check) now tells "the in-flight
error IS a defer failure" from "an original error a defer may replace" -
an in-flight defer failure is the chain's first, later failures drop
against it. first-error accounting (capture/restore of the whole error
lane, value gc-guarded across the rest of the chain). deferr rides the
same lanes (one tag, compile-time distinction only). oom stays the
stop-everything lane. defer_cascade + nil_pending_defers removed. the
one livelock found en route (pre-captured in-flight reported as a new
failure -> the dying-frame loop restarted without popping) is split into
accounting vs control flags, pinned by construction. tests:
defer_cascade.ok (the old halt semantics) replaced by
defer_continuation.ok + defer_continuation_uncaught.ok +
coros/defers_continuation.ok. the defer-owning try still cannot catch
its own defer's failure (the consumed-record ruling, unchanged).
gates: 425/425 on plain, threaded, asan, aggressive-gc.

### 40 — rename: Class -> type, Object -> object

the two hierarchy roots join the builtin classes' lowercase convention
(str/num/bool/array/map/function/nil/coro, and now object + type): the
seeds table, the interned spellings, the one internal error message that
named the root, the doc comments, and the five tests that used the old
names. object is the root (isinstance(x, object) is true for
everything); type is the class of every class value (classof(num) ===
type, the one self-loop). prints follow: <class object>, <class type>.
Q ruled this alongside the coro snake_case lean (R24). gates: 425/425
x4.

### 41 — universal string interning (D20)

every ok string is canonical: literals, concat results, conversions,
class/method names, host-created values all route through the interner
via ONE choke point (create_object_string, reaching it through the gc's
watch - alloc->gc->interner; the interner's own miss path takes the new
bare creator create_object_string_uninterned so there is no re-entry).
'a' === 'a' is true; === itself is unchanged raw word identity. the
table becomes a WEAK set: mark_interner deleted, sweep grows a pre-pass
that removes unmarked entries (string_interner_remove_at - the okmap
family backward-shift, no tombstones) before the object walk frees
them, so dead canonicals never dangle. the caller rooting discipline is
unchanged: fresh vs unmarked-table-entry were always the same contract.
comments updated where the old doctrine was load-bearing (the literal
site's C11 note, the interner header, native_coro_status, the
method_map probe note). tests: strings/interning.ok +
strings/interning_churn.ok (churn + rebirth + map round trip), and the
two old-doctrine expectations flipped (classes/identity.ok,
loops/fused_conditions.ok seq-same). 300k churned strings in 0.55s
under a 64mb cap: the table pins nothing. gates: 427/427 x5.

### 42 — the coro C API + ok_native_suspend (D21)

the vm_tick report's answer, landed: ok.h grows the embedder coro
surface (ok_coro_new / ok_coro_resume / ok_coro_status / ok_coro_current
+ ok_value_is_coro + the OK_CORO_* enum), wrappers over coro_drive with
ok_call's exact C boundary discipline (errors -> ok_last_error at the
host boundary, pending lane inside a native; R21 dead-answers-null; the
coro gc-guarded across the drive so an unrooted host value survives it).
the one real piece: ok_native_suspend - a native parks its caller at its
own return boundary. a flag on the execution state, set at request time
(validated: coro owner, no barrier, depth 1 - refusals use the yield
error family spelled "suspend"), consumed by the six native
call-completion arms (OP_CALL, OP_CALL_LONG, OP_CALL_SPREAD,
OP_CALL_SPREAD_LONG, OP_INVOKE, OP_SUPER_INVOKE): the native's result
goes out to the resumer, the coro parks exactly like OP_YIELD, the next
resume's argument lands in the call's result register. request-time
validation makes the flag unable to outlive its flight. tests/capi/ is
new - a pure C99 harness (140 checks: the two-way channel, R21/R22 from
C, the park at plain/spread/method/defer shapes, a suspended coro
escaping its dead creator, self-resume refusal, the three barrier
refusals); doubles as OR-3's memory-backed-loader test bed. ALSO in this
patch: the embed test's one `Object` -> `object` (patch 40's rename
missed it; the gates had been quoting e2e only, so it rotted silently -
the unit runner lane is back in the receipts). gates: e2e 427/427 x5
(plain, threaded, asan, aggressive, asan+aggressive) + unit x3 / embed /
capi x5 (gcc, asan, asan-aggr, threaded-asan, threaded-asan-aggr);
delta blue 14065400 correct, flat (617.8 -> 614.4ms median -O0, 251.5ms
-O2 vs session-33's 249.7 which was -O2 all along).
