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

### 43 — modules: the compile-time spine (OR-1, "O Resolution")

vs coro a8c4a99 (interning + Q's coro C API landing). MODULES.md
section 10's OR-1, whole: the three import forms (import "p";
import "p" as a; from "p" import x, y as z;) - quoted paths only
(M1, bare identifiers get the teaching error), module top level only,
pub is the export rule (M9: pub let/fu/class + pub span lets; glob
cannot be pub). the module system (src/okmodules.c/h, new): the
loader vtable (read_source today - the CLI's fs default is installed
at ok_create; load_native arrives with OR-3), the resolution ladder
(exact / +.ok / /index.ok, native candidates detected and refused
with the OR-3 error), file-relative + OKPATH lanes, realpath'd
canonical registry keys (D20-interns the class names), depth-first
loading with the 128-deep chain guard, cycles and path conflicts as
hard errors with teaching notes (the cycle walks its chain), the 404
that lists every location and form tried. the module value: a sealed
namespace instance (module_namespace classes - public reads, refused
writes, prints <module /path>), one immutable field per pub export,
harvested from hidden globals (" module:<canon>:<name>") the export
epilogue stores after the module's top level ran (the globals trick
one level up; an early top-level return is refused so the epilogue
always runs). binding (M5): import "a/b" binds the root a with the
module at a.b, same-root imports merge, a single-segment path binds
the module itself, as binds the module value, from-imports bind
exported symbols (probed against the frozen namespace, pub only).
std is reserved in module-world TUs (M8); a standalone script keeps
every name. per-module source registration keeps multi-file traces
honest (the stale okvm.h 0.7.0-registry comment finally told the
truth and was rewritten). the VM's OP_INVOKE namespace lane routes
non-closure callables through call_value: mod.Class(args) is
construction, and OR-3's natives will ride the same lane. struct ok
moved to okstate.h (internal visibility for the driver). the e2e
runner extension: a directory holding main.ok is ONE test case
(fixtures skipped), a // okpath: directive prepends OKPATH - both
runners (C++ and the sandbox gate). tests: 44 new cases across
tests/e2e/tests/modules/ (chains, diamond dedup, a 20-deep chain,
init order, root merging + conflicts, cycles incl. self, alias/from,
privacy, missing exports, immutability, span exports, captured
upvalues, pub classes, glob injection, cross-file traces, module
parse/top-level failures, early return, okpath, the native hint,
std reservation x3, and every syntax refusal). 427 -> 471. gates:
471/471 x5 (plain, threaded, asan, aggressive, asan+aggressive; the
aggressive lane earned its keep - it caught gc_init not nulling the
new modules watch pointer, the module system initializing after
vm_init's allocations, and the compiled module function riding
unrooted across vm_begin_source; also fixed en route: a guard-stack
leak in the namespace builders, imports binding through the
uninitialized-local state, and directories passing the readability
probe). asan leak-clean (the registry owns its canonical paths now).
delta blue unchanged (1.4e7 in 0.62s).

### 44 — tree-sitter: imports, pub, and the spans catch-up

REGENERATED (session 37): Q's apply failed because the original was cut
against master + 29 and the tree-sitter repo never took 29. this one is
SELF-CONTAINED against clean master 72b061f: it folds 29 (defer/deferr)
in, so it applies directly with no prerequisites. if 29 is already
applied somewhere, use the previous version from git history instead. the import grammar catches up to OR-1: import_decl
reshaped (no declmods - an import is its own statement kind now;
glob import is a hard error in the language, the old corpus test
rewritten), from_import_decl + import_specifier new, pub added to the
top-level let/fu/class declmods, import_decl dropped from the
top-level conflicts list (nothing shares its prefix anymore). AND the
spans catch-up that OR-1 made load-bearing: span_bindings (let (a,
mut b) = call(); - the destructuring let, trailing comma legal, the
value must be a call) and the multi-value return (return a, b;) -
both landed in the language at v0.3.0 and the grammar never knew; the
pub span-let export form parses them together now. queries: "from"
joins "import" @include, import_specifier aliases highlight like
import aliases. corpus: modules.txt + spans.txt (6 tests). generated
files regenerated with tree-sitter-cli 0.27.1 (same version as 29 -
parser.c churns wholesale, the semantic delta is grammar + queries +
corpus). verification: corpus 138/138; every valid module e2e fixture
parses with zero ERROR nodes (the four intentional invalid-syntax
tests - star imports, unquoted paths - correctly error); full e2e
tree sweep goes from 175 error files on the base to 104 (the span
family fixed). the remaining 104 are pre-existing debt the grammar is
versions behind on: varargs, two-binding for-in, classes
operators/statics/details, coros, is/conversions. a full catch-up is
its own session - Q rules on when.

### 45 — the diagnostic doctrine (D22)

Q's ruling from the OR-1 landing review, now standing for every
diagnostic: the helpful half only (no editorial asides, no draft
references like M9/OR-3 in user-facing text), and standard punctuation
only: comma, colon, period. no dash, no em dash, no semicolon (a
semicolon inside a quoted syntax example is code, not prose, and is
legal). thirteen messages swept: the six OR-1 offenders (the two
"python-plain" notes lose their editorial tail and keep the spelling
hint, the chain-limit drops "design smell", the std/native/export
messages drop their OR-numbers) plus seven pre-existing ones (the empty
map literal, the two overloadability notes, the compound-operator
message, the span-let field note, the two sealed-class messages). two
e2e assertions updated with their messages. applies on modules-staging
after 43. gates: 471/471 x5.

### 46 — std: the re-home (OR-2)

the std root is real: `import "std/array"` and friends resolve against
a built-in registry that never touches the filesystem, OKPATH included
(M8: nothing can shadow the standard library, the shadow test plants a
poisoned std/array.ok on OKPATH and the builtin still answers). the
table today: std.array (len/push/pop/remove), std.map (len/remove/get),
std.io (print, the function face of the statement, same formatting and
the same hook, %g included), std.math and std.str importable and empty
until the stdlib growth version fills them (from-imports of missing
members get the ordinary no-pub-export teaching error). the members are
the SAME native function objects the flat spellings use, harvested from
their hidden globals into the same sealed-namespace shape file modules
get, so a std module value IS a module value: frozen, printable as
<module std/array>, passable, boxable, from-importable. built lazily on
first import (an embedder that never imports std never pays), rooted
through the module registry, keyed "std/<stem>" which cannot collide
with any realpath'd file key (those are always absolute).

one grammar delta the tests forced: `print` is a keyword and std.io
exports it, so member position after '.' and from-import names accept
the print keyword (aliases stay plain identifiers) - the same special-
casing `operator` already gets in member position.

the flat surface stays as deprecated aliases per the ruling: the seeds
(len/push/pop/remove/get/classof/isinstance), the method spellings
(a.len()), and the print statement. old scripts are unchanged, the
removal is the next stdlib version's call. the deprecation plan and the
%g default's replacement by std.io's formatting are recorded in
MODULES.md's OR-2 section as that version's scope.

M8's root reservation now exempts the std/ lane only: `import
"std/array"` binds the std root, a user file or OKPATH path whose first
segment is std still gets the hard error. the bare `import "std"` is
refused with a teaching message, and unknown std modules list the real
table.

applies on modules-staging after 43 + 45. gates: e2e 481/481 x5
(10 new std tests: merge, io, empty modules, from-imports + aliases,
missing member, bare root, unknown module, immutability, the OKPATH
shadow, std inside file-module chains, value passing; std_not_module_yet
retired), unit x3, embed x3, capi 140 x3, delta blue flat (1.40654e7 in
0.64s).

## 47 — module allocators + the registry read api + the surface pins

Q's landing review caught it in one skim: okmodules.c was full of plain
mallocs. the codebase's discipline is the user-provided ok_allocators
(every byte the vm touches goes through them) and the module spine rode
libc instead. this patch routes EVERYTHING through the user's table: the
path plumbing (joins, ladders, traces, the name arena, the pending tree,
the segment buffers), the registry's canonical paths and import lists,
and the fs loader's read buffers - the loader contract is now explicit
(okmodules.h): a read_source buffer must come from the ok_allocators the
instance was created with, the instance frees it with its release call.
the one libc free left is realpath's own buffer (libc allocates it,
libc releases it, the registry only ever frees what IT allocated: a
realpath hit is copied onto the user's table first). the read side is
api_surface too now (Q: "there should be an api for modules
export/imports otherwise how is cli gonna even read the exported
table?"): ok.h grows the module registry read - ok_module_count/path/
value/find (what got loaded, load order, keyed by canonical path,
"std/<stem>", or a force-loaded native's declared name),
ok_module_import_count/import_path (each module's own graph edges,
deduped, load order), and ok_module_export_count/name/get over any
module value (declaration order, the namespace's own_names). the
registry record grows the imports list; ok_modules_prepare hands the
TU's resolved edges to its caller (the record, or the entry run's
discard).

the round's other question, answered with tests instead of words: glob
and pub were never hidden from the surface. `glob let g = <module
value>` works in every shape (the module itself, the same name, a
multi-segment root namespace, from-imported members, read back from C
through a global), and `pub let re = <imported member>` re-exports
freely - only the SAME name is the ordinary duplicate-local rule (M13),
which is exactly what Q expected ("we cant bind the same name, but we
can bind to another name"). two fixture dirs (surfaceglob, surfacepub)
pin all of it so it can never regress silently.

applies on modules-staging after 46. gates: e2e 483/483 x4 (2 new
fixture dirs), unit x3, embed x3, capi 170 x1 (30 new checks: the
registry read over the std lane, glob-of-module read back from C).

## 48 — OR-3: sandbox, natives, the embedder loader

loading is born gated, the whole plan of it (Q: "right when we allow
for loading (that is now!)"). four pieces, one patch.

the capability system: ok_specs grows caps_dropped (0, the {0} default,
refuses nothing - every existing embedder keeps the full surface;
OK_CAP_ALL is the locked stance) and the ok_dylib hooks; ok.h grows
ok_caps_get (the granted set) and ok_caps_drop (the ratchet: drop only,
never grant, nothing can shrink the mask - the security property by
construction). three families (NATIVE_LOAD, NATIVE_BUILD, OR-4's half,
HOST for fs/env/clock as they land). the CLI maps them: --restrict
(drops load+build+host), --sandbox=locked (everything), granular
--deny=native,build / --allow=host (parse-time composition, the ratchet
untouched), and the native probe itself is SKIPPED under a dropped
OK_CAP_NATIVE_LOAD - the 404 teaches why.

ok_module.h (new, include/ok/): the native ABI. OK_API (msvc
dllexport/dllimport, default visibility elsewhere), the
ok_native_module descriptor (abi, name, version, fn table, mount, on_
load/on_unload), ok_type_tag, and the ok_api vtable - Q's ruling, "we
should support everything we have and expose to the abi": the write
side + value_null, the read side + value_as_native_value, classof/
is_instance/class_name, call, the class door, native_error/throw, root/
unroot, the coro primitives incl native_suspend, and two entries the
plan did not list that the first real module could not live without:
view (string-view construction - a native cannot even spell its
arguments without it) and module_export (the mount callback's export
primitive: classes and constants through api->module_export, the
namespace freezes when mount returns). the table is STATIC and
STATELESS (every entry routes through its ok* argument): a module
stashes it at export time, it stays valid for the process lifetime,
several vms coexist. OK_NATIVE_ABI is a single integer (1), checked at
load; the module checks the host back (the sample demonstrates).
append-only entries, no semver, "rebuild your natives" is the honest
pre-1.0 stance.

the loader: the vtable grows load_native (false + empty err = clean
miss, false + a message = a native EXISTS but is broken, the hard
error; the default lane stats before opening so ordinary misses stay
clean). the ladder probes ok-module candidates by ASKING THE LOADER TO
READ THEM - the library never stats or opens files itself anymore (the
old ladder's stat() violated M14 for memory-backed loaders, the capi
test proves the fix: a three-module virtual graph over "mem:/" paths,
zero filesystem). natives are first-class at every rung (M6): the
ladder probes base+ext (and the exact path when the import spells the
extension), canonicalizes, checks the registry, then loads. loading =
open (RTLD_NOW|RTLD_LOCAL, LoadLibrary on windows, host hooks first,
platform wrapper default, nothing on platforms with nothing) + resolve
ok_native_export + hand it the api table. then register_native: abi
check, name check (the descriptor's name must equal the import stem),
on_load (a failure fires on_unload), fn staging + the mount window
(marked for the WHOLE staging+mount+build stretch - the aggressive
lane caught exactly that window sweeping staged values), duplicate
export names refuse (M13), the sealed namespace, the registry record
(is_native, the descriptor for the on_unload sweep at ok_free, the
default lane's handles close after it, never mid-run).

ok run --native m.so app.ok: ok_native_force_load registers under the
declared name, bare imports resolve to it (the registry probe is
length-bounded - the import literal is a lexer slice, never a C
string). ok_set_loader installs a host loader wholesale;
ok_load_native_default is exposed for composition (custom sources, the
stock native lane).

receipts: the sample module (tests/e2e/tests/modules/native_hello:
one function, one class, one constant, on_load observable) runs end to
end - import "./hello", from-imports, Greeter() construction, method
dispatch; abi-mismatch and name-mismatch fixtures print their messages;
--restrict refuses with the capability error; the capi harness grows a
memory-backed loader section (201 checks now: the virtual graph, the
registry read over virtual keys, the ratchet, a locked instance still
computing). the fixtures declare raw gcc build commands (build.sh, M12:
whatever toolchain) and both runners (bash + the C++ main) run them
first, plus a new `// okflags:` directive (the okpath precedent) so
per-test CLI flags exist. the old "native loading is not available
yet" lane is retired (natives load now); a broken candidate (a text
file named .so, a corrupt image, a missing export) is the hard error
with the loader's own reason as the note.

applies on modules-staging after 47. gates: e2e 488/488 x7 (plain,
threaded, asan, aggressive, asan+aggr, threaded-asan,
threaded-asan-aggr; 5 new native fixtures), unit x3, embed x3, capi
201 x5, delta blue flat (1.40654e7 in 0.62s).

## 49 — native class fields (the access half of the state question)

Q's state question, scoped: per-instance STATE on C-defined classes
needed the access lane, not a new storage plane. ok.h grows
ok_class_add_field + ok_value_get/set_field; the data plane is the
vm's own field slots, gc-traced by the ordinary walk, no getter
round-trips through C storage. the C lane is trusted (the ring is
script doctrine), mut is honored, module namespaces refuse writes.
the native_state fixture is the proof: a Counter with payload + step
fields, driven and subclassed from script.

[reconstructed entry - the session's sandbox died before its README
line was written; the lane receipts bracket it: 488x7 at 48, 501x7 at
50.] applies on modules-staging after 48.

## 50 — OR-4: project.ok + the build pipeline

project.ok is strictly ok (M10): a plain module evaluated in a fresh
instance with the full stdlib and NO natives, exporting one pub table
- name, version, ok, modules (import roots), deps (path deps only,
M11: no registry, git clone IS the dependency manager), natives (dir,
build command, artifact; whatever toolchain, M12). discovery walks UP
from the ENTRY FILE (never cwd - the last implicit working-dir
dependence dies there) to the nearest project.ok; that root governs.
the resolution ladder, natives first-class at every rung (M6):
relative, std (never the path walk), the governing project's roots,
its native targets (built if stale and allowed), its deps by name
(recursing into dep projects), OKPATH last. the 404 teaches: the full
trace of every location and form tried. ok build [--clean|--force]
runs the declared commands and stamps .okbuild/<target>.stamp;
auto-build-on-import asks consent the first time (--yes/--no-build,
the tty prompt); a bare script NEVER builds anything. ok where
"mathx" prints the resolution trace without running. the manifest
harvest rides public machinery end to end: ok_module_evaluate (the
standalone lane), ok_map_pair_at (the keyed-table read the harvest
needed), the governing project matched by deepest-root prefix against
the installed graph. the fixture farm: 14 dirs, every failure mode
covered (missing deps fail eagerly, dep/native name collisions name
both (M13), cycles name their chain, restricted mode refuses with the
capability error, where404 teaches).

applies on modules-staging after 49. gates: the lane at 501x7 (488 ->
501 with the fixture farm), the rest of the matrix green, delta blue
flat.

## 51 — pub members: fields join the ring

Q's ruling: "yes this what i mean member publicity in scripts (in a
way that does not comprimise the current design)". methods carried
pub since Citadel; fields were the gap, and the workaround was
getters - "so we dont have to make getters and setters and lose perf
there". pub let / pub let mut / pub static let now compose on
class-body fields exactly like pub fu: readable outside the defining
hierarchy (and writable with mut), module boundaries included, a slot
load on both sides of the module line. private stays the default; the
ring is unchanged; pub let x without mut reads from anywhere and
writes nowhere (the mut error, not a visibility error).

the mechanics: the pub flag rides the instruction word's spare R bit
(64-bit word2 had exactly one free bit after OP/P/A/B/C/Q/CMP),
carried by OP_DEFINE_FIELD and OP_DEFINE_STATIC_FIELD, decoded into
field_entry.pub, consulted by the five vm access lanes before the ring
check. the C lane grows ok_class_add_field_ex (ok.h + the vtable's
appended tail - the one abi 3 addition); the old entry stays as the
private default. the module namespaces declare their exports pub
explicitly (doctrine made honest).

two en-route fixes, both caught by the gates, both reproducing on the
patch-50 head: the stamp learns the abi (.okbuild stamps record the
host's native abi line next to the build command - the 2 -> 3 bump
left a green stamp over a stale .so; an ok upgrade now invalidates
native artifacts), and the mount guard (api->module_export guards the
value across the staging append - the window between "the C callback
holds the class" and "the marked mount array holds it" was always
collectable; the fn-staging loop always had this guard, the
mount-callback lane did not).

fixtures: pub_fields, pub_statics (+ both immutable-error pins), the
cross-module pubmembers pair, native_pubfield (a C class with pub x/y
from script), and the capi section (an Open class driven from C, 39
checks).

applies on modules-staging after 50 (fcecbaa). gates: e2e 510x7, unit
x3, embed x3, capi 324x5, delta blue correct at 1.40654e7. verified
apply-clean in sequence with 52 on the patch-50 head in a scratch
worktree.

## 52 — the ABI goes semver-shaped (the compat window)

Q's ask: "since we are approaching v1, that means any bump to the
number in the abi requires a full major update right? this is bad, we
have to make it compatible, like versioning in semver combat way so we
dont get forced to v2 when we just want to incrementally add something
right?" - and the append-only discipline kept since OR-3 had already
paid for it. OK_ABI_MAJOR.OK_ABI_MINOR, packed
((major << 16) | minor). major breaks: reshuffled vtable, changed
signature, moved field, renumbered tag - the rebuild message. minor is
additive only: appended vtable entries, appended descriptor tails; the
loader takes any module with the same major and a minor <= the host's,
a newer minor refuses with the upgrade message, minor 0 is a zeroed
descriptor, a rebuild. the encoding keeps history intact: abi 1/2/3
decode as (0, n) and load. v1.0.0 freezes (1, 0) and the compat window
opens for real: every 1.x ok runs every 1.y module with y <= x, and no
addition ever forces a v2. both directions hold: a module compares
p_api->abi against OK_NATIVE_ABI exactly as before, or decodes the
pair for a softer stance.

the proof runs on every gate: abiminor_old is a fixture module that
hardcodes .abi = 2 (frozen in the patch-49 era) - it loads and runs
unchanged on the (0, 3) host. badmajor declares (1, 0) and takes the
rebuild message; badabi's 99 is now the newer-than-host lane.

applies on modules-staging after 51. gates: the pair landed together -
e2e 510x7, capi 324x5, unit + embed green, delta blue correct at
1.40654e7.

## 53 — P1, the honesty warmup (0.9.0 Reconciliation opens)

the roadmap's P1, delivered as ruled: the version stamp, `ok create`,
OK_ROOT, and the c99 -Werror gate with the warning backlog dead.

the stamp (include/ok/ok_version.h, new): one header, the single
source of truth - the three numbers, the lane ("rc.1" while the wave
is in flight, "" on release day), and the composed string.
`ok --version` / `ok -v` / `ok version` print one greppable line:
`ok 0.9.0-rc.1 (abi 0.3)` - the abi rides ok_module.h's patch-52
encoding; the truth stays where it lives. the capi harness re-composes
the string from the numbers and compares (the 325th check): drift
between them fails the gate. the bench workflow's `--version ... ||
true` tolerance dropped the same patch.

`ok create <name>`: the scaffold Q renamed ("in cli i like this, yes
we should make it") - project.ok (name + version, the keys the CLI
reads), src/main.ok, tests/, .gitignore (.okbuild/), standing on
OR-4's discovery. refuses existing dirs, path separators, dotfiles;
the scaffold runs (`ok src/main.ok` prints hello) and its manifest
answers `ok where`.

OK_ROOT (Q's flag, the test-path fix): the harnesses stop reaching
for ../../ok. the CLI exports OK_ROOT to every declared-build child
(an inherited OK_ROOT always wins; the compiled-in checkout prefix is
the fallback - cmake and the sandbox build both bake it); the C++ e2e
runner exports it for fixture builds and ok runs alike; every
-I../../../../../include in the fixture farm is now
-I$OK_ROOT/include. the relative reaches are dead.

the gate + the sweep: -std=c99 -pedantic -Wall -Wextra -Werror
(-Wno-missing-braces rides along - the flat-initializer idiom over
the tagged index types, C23's {} is the eventual home; the two
anonymous unions, okindex.h and lvalue, are the one chosen C11-ism,
pragma-marked at their definitions with the ruling in the comment).
the threaded lane keeps -Werror but drops -pedantic: labels-as-values
is the documented extension (OK_PURE is the strict gate). 530
warnings -> 0, among them: OK_CAP_ALL becomes a macro (enumerator
values must fit int), the fused-condition width guard was dead
(ok_index and the 2w operands are both 16-bit - the window is
structural; the guard deleted with a note), the classic dispatch
switch now spells out the four pre-patch NAME opcodes + OP__COUNT
mirroring the threaded table's NOP aliasing (byte-identical by
design; -Wswitch asked), a -O2 maybe-uninitialized in the seed switch
got a refusing default arm, three %p casts in okgc's paranoid lane,
and the dead code deleted: okparser's current/peek/get_precedence
declarations, okstring's remove_flag, okvm's run_pending_defers
wrapper, a never-jumped `fail:` label, six unused locals.

tests: the new CLI lane (tests/cli/run.sh, 10 checks: the three
version spellings' shape, -v's precedence, the scaffold's four
pieces, the scaffolded run, the two refusals, discovery through the
scaffold's manifest, the unknown-flag exit) wired into CI beside e2e;
the capi composition check.

applies on modules-staging after 52 (973bcd0; the commit is 257c189).
gates: 9/9 build flavors under -Werror, e2e 510x7, unit x3 + embed
x3, capi 325x5, cli 10/10, delta blue correct at 1.40654e7
(1.96x/1.88x of wren - the standing 1.93x within run noise).

## 54 — P2, the utf-8 core (codepoint-first ok)

the codepoint-first amendment, as ruled: "thats for iteration + index",
len = codepoints, the front door, and the string-scanner rework with
\u{} escapes.

ok_utf8 (src/okutf8.h, new, header-only): the C99 port of the ok::utf8
sketch with its FIXME resolved into the contract - decode answers 0 on
malformed input (validated-region callers may assert), validate is the
ONLY entry that may see bad bytes, and failure carries the byte offset.
the check table is the ruled one: overlong C0/C1 + E0 80-9F + F0 80-8F,
surrogates ED A0-BF, out-of-range F5+ and F4 90+, stray/truncated
continuations. validate additionally rejects raw NUL, the bidi controls
(U+202A-202E, U+2066-2069 - the trojan-source hole), and C0 other than
\t \n \r. count_lossy implements the runtime-string ruling: one U+FFFD
per malformed byte, never a crash, never a rejection at construction.
encode backs the \u{} escapes.

the front door: parser_parse validates the whole source before any
lexing - entry compile, REPL lines, and module loads all funnel through
it, so nothing else changes. the rejection is a located parse error
(line + column walked from the offset, the reason as the note). C-string
sources end at their first NUL by construction, so an embedded NUL
truncates (standing semantics, unchanged); length-carrying loaders can
gate the full region later. the lexer never learns unicode exists:
identifiers accept any well-formed non-ascii codepoint (π, 名字, λ all
test-pinned).

the str hidden shape: ITER_PREP/GET/LOOP grow the OBJ_STRING arms - a
utf8 byte cursor against the byte end (immutable, so no map-style
version check), GET decodes and interns the one-codepoint piece (the
U+FFFD lane for malformed runtime bytes), LOOP re-decodes and advances.
s[i] is the i-th codepoint as a fresh string (INDEX_GET/GET_LONG arms;
INDEX_SET refuses - strings are immutable, concatenation exists). len
answers codepoints: the count is lossy-cached on the interned header at
creation (object_string.codepoints, beside the hash - strings are
immutable, the cache is free), the free spelling and str.len() (the
class's first method - the sealed-empty 0.5.0 doctrine retired by the
ruling) both read it. byte length stays string_get_length behind its
own door until an s.bytes() ever earns its keep.

the string-scanner rework (shared with P3, as the roadmap drew it): the
scanner learns only that a backslash never terminates (escaped quotes
and newlines ride through raw); the parser's unescape lane owns the
meaning - \n \t \r \\ \' \" and \u{1-6 hex digits} (range- and
surrogate-checked, encoded through ok_utf8_encode). unknown escapes,
dangling escapes, and bad \u{} shapes are located compile errors - the
diagnostic doctrine. literals without a backslash stay source slices
(zero copies); the translated bytes live on the AST node.

the find: `for let i = 0 : len(a)` - a call whose dest sits BELOW other
locals breaks the call window's contiguity (callable at dest, args at
dest+1.., but args stage above ALL locals). the numeric for's hidden
limit/step locals were the natural victim: the limit slot read the
uninitialized register above it as the argument. pre-existing since the
register VM, exposed by P2's conformance check (the hidden shape vs the
manual index walk). the three for slots (start/limit/step) now compile
through a temp and move; the let initializer never had the shape (its
local is top-of-locals at init). also: the project_ambiguous fixture's
vendor/ray directory was never committable as an empty dir and had
vanished with an old sandbox - restored with a .gitkeep so the M13
ambiguity check is reachable again.

tests: iterate/index/len/escapes/unicode_id (25 checks across the new
surface, the conformance pair included), 13 error fixtures (bounds,
whole-number, immutability, the escape error family, the two-binding
refusal), 4 binary gate fixtures (0xFF, bidi, C0, overlong - written
with printf, NUL-free so the runner's grep stays text-mode), and
oktest_utf8 (16 unit tests, 54 checks: the check table, the offsets,
the lossy counts, the encode round trips). superseded pins retired:
maps/errors/iterate_string (strings iterate now - the ruling),
classes/native_method_error repurposed to a genuinely-unknown method.

applies on modules-staging after 53 (257c189; the commit is c21c7e9).
gates: 9/9 flavors under -Werror, e2e 530x7, unit 5x3 (54 checks) +
embed x3, capi 325x5, cli 10/10, delta blue correct at 1.40654e7
(1.91x/1.86x of wren - flat against the standing 1.93x; the codepoint
cache is free).

## 55 — P3 part one: f"" interpolation + m""/fm"" multiline

the strings round opens with the ruled interpolation made real: f"" with
${} regions, m"" multiline, fm"" both at once. LANDED by Q's hand on
reconciliation as f05445a - verified zero-drift (the deliverable diffs
empty against the landed commit), gates re-run on the landing itself.

the shape: an f-string stays ONE token, and its interpolation extents
come from ONE byte scanner both the lexer and the parser ride
(ok_fstring_region_end) - braces nest, quoted strings and line comments
hide theirs, and a quote riding an f/fm prefix inside a region takes
the same scan recursively, so `f"${s("}")}"` and friends work and the
extents can never drift from the token's own boundaries. the parser
re-opens the token, chunk-splits the body, sub-parses each ${...}
region with a region-anchored lexer (lexer_init_region: the closing
brace is the sub-parse's EOF wall, positions stay absolute - errors
point at real source), and desugars the whole literal to string +
to_string(expr) chains over the existing infix ADD. OP_TOSTRING/
OP_TOSTRING_LONG are exactly the `as str` builtin lane in opcode form
(perform_to_string -> as_str: it allocates but never runs nested
script, so the caller needs no REFRESH_AFTER_NESTED; every as_str arm
makes its final allocation last), constant operands included -
f"${1}" never materializes the 1.

the rulings, all delivered as ruled: `\${` is the literal dollar-brace
in the f lanes only (plain strings and m"" keep the P2 escape set and
refuse `\$`), `$` without `{` is just a dollar, braces are literal in
bodies, NO format-spec language - `:` after the expression is a located
reserved error, not a mini-DSL - mf" stays the identifier mf, the
unterminated errors name the opening line, and newline-in-single-line
still dies. the m"" opening-line and line-anchoring rulings are pinned
by substring on the reported line:col.

tests: 14 new fixtures under strings/ - fstring (basics + escapes +
nesting), mstring (multiline bodies), fmstring (both together), and 9
error lanes (dollar_escape_refused, empty_interpolation,
format_spec_reserved, junk_after_expression, mf_is_an_identifier,
newline_in_single_line, unknown_chunk_escape,
unterminated_interpolation, unterminated_mstring).

applies on reconciliation after 057309c (P1+P2's landing); the landed
commit is f05445a, byte-identical to the deliverable. gates (run on
f05445a): 9/9 flavors under -Werror, e2e 544x7 (530 + the 14), unit +
embed 5x3, capi 325x5, cli 10/10, delta blue correct at 1.40654e7
(1.94x/1.88x of wren - the standing 1.93x within run noise).

## 56 — P3 complete: the str method toolkit + std.math + the deprecation windows

the strings round closes: the ruled toolkit lands as methods on the str
class (std.str stays dead, the class surface is the canonical surface),
std.math opens with the deterministic cmath surface + the RULED idiv,
and the 0.9.0 deprecation windows open in oklang's README (docs-side,
the removals are v1.0.0's to schedule).

the toolkit (12 methods, receiver-first, no free spellings, no hidden
globals - the retier direction): trim (both ends, the six ASCII
whitespaces), split (non-empty separator required, no implicit
whitespace lane, n+1 pieces for n occurrences), join (the receiver is
the separator; every member must already be a string - the `as str`
lane is explicit, join never rides it implicitly), contains /
starts_with / ends_with (byte-exact memcmp shapes, empty matches
vacuous-true), index_of (CODEPOINT answers, the s[i] unit; absent is
null, the get(m, k) convention - not a -1 that arithmetic would
silently eat), upper / lower (the ASCII fold only - full case mapping
wants tables the core will not carry; every other byte rides through),
repeat (whole non-negative count, overflow-guarded), and the
byte/codepoint pair: bytes() is the embedder's byte door surfaced at
last, codepoints() is len's explicit spelling completing the pair.

matching is byte-exact over the utf-8 payload - an occurrence of a
well-formed needle always lands on codepoint boundaries, so no decode
is needed to find and no piece can split a codepoint (multibyte split
and index_of pins ride the fixtures).

std.math: 20 members behind the module door only (hidden globals, the
print_fn precedent - math is not any class's surface). the surface:
abs floor ceil sqrt exp log log2 log10 sin cos tan asin acos atan sinh
cosh tanh (one X-macro table, one body), plus pow, atan2, and the RULED
idiv - trunc toward zero, a hard error on the zero divisor, no operator
(the // slot is comments forever). "deterministic" is the ruling's word
and it means finite in, finite out: every domain violation (sqrt(-1),
log(0), asin(2)) and every overflow lane (pow(0, -1), exp(10000)) is a
located hard error; no NaN or infinity ever crosses back into script,
where the arithmetic lanes would coerce them silently. idiv's whole
operands inside the safe range divide through int64 exactly; huge or
fractional operands fall to trunc(a / b) with the f64 rounding carried
and documented.

the GC discipline for allocating natives (split builds an array of
strings): the array is guarded across the whole loop, each piece is
guarded between its creation and its append (LIFO pairs, the guard
stack composes); the one-shot builders (join, upper, lower, repeat)
follow the concat lane's shape - everything in C memory first, the
string object is the final allocation. the aggressive lane (collect on
every allocation) exercised all of it.

tests: 16 new fixtures (544 -> 560): strings/toolkit.ok (the happy
surface, multibyte pins, f-string interop), modules/std/math.ok (all 20
members, both door spellings, from-import, the module print shape), and
14 error lanes (split's empty separator + non-string separator, join's
non-array + non-string member, repeat's negative/fraction/non-number,
math's type error + the domain shapes + the result-range shapes + idiv's
zero divisor). one superseded pin retired honestly: from_missing.ok
expected std/math to lack floor - it has floor now; repurposed to pi,
which also documents the no-constants state.

applies on reconciliation at f05445a (patch 55's landing; the replay
receipt: applies clean). gates: 9/9 flavors under -Werror, e2e 560x7,
unit + embed 5x3, capi 325x5, cli 10/10, delta blue correct at 1.40654e7
(1.92x/1.87x of wren - flat against the standing 1.93x; the toolkit
adds no dispatch work, the numbers agree).

OPEN for Q: (1) math constants (pi, e) need a registry lane for
non-native members - worth one? (2) the toolkit's semantic calls are
mine, documented above: split's required separator, join's strings-only
membership, index_of's null-for-absent, the ASCII-only case fold. veto
any and patch 57 amends.

## 57 — the math constants lane (pi, e join the registry)

Q's answer came back same-day: worth it before v1 "if it is not very
complex to make". it was not complex, because the registry turned out
to be value-blind: build_namespace harvests hidden globals into the
namespace's field slots without ever asking what a value IS, so the
"registry lane for non-native members" needed no new machinery - two
table rows pointing at hidden NUMBER globals, the same leading-space
lane the natives ride (the print_fn precedent holds: no flat pi, no
pi(), the module door only).

the constants themselves are spelled to full precision in ok.c
(OK_MATH_PI, OK_MATH_E) because the C library's M_PI family is POSIX,
not c99, and the strict gate hides it - and both literals were verified
against the library's own doubles at patch time (both round to the
correctly-rounded values). ok_define_math_natives grew the two numbers
and got the honest rename: ok_define_math_members. the struct comment
in okmodules.c now says the truth it always lived: "a member is a
value, native or not."

the OPEN pin resolved honestly: from_missing.ok expected std/math to
lack pi (the 56 marker). pi exists now, so the pin moved to nan - and
nan is the PERMANENT spelling: the no-NaN doctrine (finite in, finite
out, nothing crosses back into script) means std/math can never export
it. the pin can never go stale again.

math.ok grows 13 pins: the values, arithmetic on them, comparisons,
floor/sin/log compositions (sin(pi) pinned at its honest 1.22465e-16 -
the double's own rounding, no fake zero; log(e) exact), both door
spellings, and the from-import door carrying them like any member.

Q's other ruling this round: the P3 semantic calls STAND as documented
(split's required separator, join's strings-only membership, index_of's
null-for-absent, the ASCII-only case fold) - "yes i agree for your
rulings for now".

receipts: e2e 560x7 (count unchanged - the pins ride the existing
fixture), unit + embed 5x3, capi 325x5, cli 10/10, delta blue correct
at 1.40654e7 and flat (1.92x/1.87x of the standing 1.93x). applies on
reconciliation at 54fd30f (patch 56's landing; replay clean).

## 58 — the complete escape set (\a \b \e \f \v \0 \xNN)

Q's question: "support for all ansi escapes like the color commands \x
and all those, shouldnt we do it all?" - yes, and it landed as one
parser-side switch extension. the scanner needed nothing: it has been
meaning-free since the P2 rework (a backslash never closes anything),
so the token shape is unchanged and the tree-sitter grammar is
untouched.

the set: \a \b \e \f \v (the C controls plus \e - 0x1B, the terminal
escape byte; the front door keeps raw control bytes out of source, so
the escape is the only door in, and the ANSI color spellings ride it:
"\x1B[31m" and "\e[31m" are both now spellable), \0 (exactly the nul
byte), and \xNN (one or two hex digits - the same bounded-count shape
as \u{}'s 1-6, the house style).

the two honest limits, both located errors: (1) \x stops at ascii
(0x7F) - a literal only ever produces valid utf-8, and a high \x byte
would build exactly the malformed utf-8 the front door refuses as raw
source; \u{} is the codepoint escape, the message says so. (2) no
octal - a digit after \0 is refused ("C's \01 silent trap"; \x01 spells
such bytes). \? stays refused like any unknown: the trigraph fossil has
no use here, and accepting GNU's \e while refusing \? is deliberate -
\e earns its keep, \? has none.

one subtlety earned its own rule: a hex-looking byte after two digits
("\x123") is exactly the ambiguity the \u{} braces exist to kill, so
\x refuses it the same way ("takes at most two hex digits" - the "at
most six" precedent), with the check ordered last so "\xC0ffee"
reports its real error (the ascii ceiling) and not the trailing-hex
coincidence.

fixtures: escapes.ok +15 (every new escape pinned against its \u{}
spelling - the \r precedent: control bytes never ride the expect lane
raw - plus the ANSI receipt itself, and len() over the whole set),
fstring_escapes.ok +2 (the chunk lane shares the unescape, so the new
escapes ride inside interpolations too), and four error lanes
(x_no_hex, x_above_ascii, x_three_digits, octal_refused).

receipts: e2e 564x7 (560 + 4), unit + embed 5x3, capi 325x5, cli
10/10, delta blue correct at 1.40654e7 and flat (1.93x/1.88x of the
standing 1.93x). applies on 57 (which applies on 54fd30f); the stacked
replay is clean.

a note for future rounds: the tool-result display strips SGR-looking
sequences ("\x1B[31m" vanishes from view, files untouched) - verify
ANSI fixture content with grep -cF, never by eye.

## 59-p4-the-host-doors-fs-sys-and-the-audit.patch

P4, the host doors, the first real OK_CAP_HOST consumers. three
abstraction layers open behind the one-door rule - ok_fs (the whole
file read with the loader's 1 GiB cap and regular-files-only probe,
write/append, the probes, remove (file or EMPTY directory - no
recursion ever), one-level mkdir, sorted listing, cwd), ok_time (wall
+ monotonic, seconds as doubles), ok_env (get/set/unset + the sorted,
deduped keys walk) - and the audit reroutes every libok caller through
them: the module loader's source read, the OKPATH getenv, the dylib
probe's stat. ok_posix.h is the one deliberate POSIX opt-in for the
new layers (the older files keep their per-file _XOPEN_SOURCE until
touched); okdiag's stderr write stays the sanctioned diagnostic door.

std.fs and std.sys join the built-in registry as the first HOST-gated
std modules: the import refuses with the ratchet's message when the
instance was born without the cap (the where lane stays a pure
resolution query). the surfaces ride the same hidden-globals harvest
as math - fs: read/write/append/exists/is_file/is_dir/remove/mkdir/
list/cwd/join (join is pure compute riding the module); sys:
env_get/env_set/env_unset/env_keys/clock/monotonic/exit.

sys.exit is the round's one new public lane: the native sets the code
then rides the error unwind as OK_EK_EXIT - uncatchable by design
(attempt_unwind declines every record for it, frames still close and
defers still fire, exactly the uncaught-error walk), the report lane
prints nothing (the run ENDED, not failed), ok_run answers the new
OK_EXITED with the code readable through ok_exit_code, and the CLI
maps it to the process status. the ratchet discipline: a failing defer
on the way out replaces the request (a real error outranks an ask).

the e2e runners (bash + C++) grow the `// expect exit: N` lane; 21
fixtures (fs.ok the self-cleaning round trip, sys.ok the env + clock
relations, 13 error pins including the NUL-path refusal where \0 does
real work, the two restrict gates, the four exit fixtures with the
defer contract pinned). capi +9 (OK_EXITED, the code, the reset lane,
the HOST gate). one gate-caught bug en route: a two-conversion pattern
fed one argument in fs.write's failure message - undefined behavior
that segfaulted the moment the missing-parent fixture ran; the splice
is two-step now, one format arg each.

applies on reconciliation after d48edf4. gates: e2e 585/585 x7 (564 ->
585), capi 334 x5 (325 -> 334), unit + embed x3, cli lane, delta blue
correct 1.40654e7, 1.95x/1.87x of wren - flat, the hot paths
untouched.
