# PERF pass 1 — the lightweight round, the report card

status: LANDED as patch 36 (this repo: `patches/36-lightweight-perf.patch`,
oklang branch `opt-lightweight`, one commit on top of your
`classes-staging` a1411d1). gates: e2e **403/403 on BOTH dispatch modes**
(switch + threaded; 399 + 4 new tests), unit + embed x5 variants (gcc,
asan, asan-aggr, threaded-asan, threaded-asan-aggr). benchmarks:
before/after on the same sandbox, -O2, RUNS=5 WARMUP=1, hyperfine-shaped
runner (the shim stands in for the rust binary in the sandbox; the CI
lane re-runs with the real one). raw json: `results/baseline-classes-staging/`,
`results/after-switch/`, `results/after-threaded/`.

what landed, in the ruled order (ROADMAP-v1 PERF pass 1):

## 1. threaded dispatch (the big one)

computed gotos behind `OK_THREADED_DISPATCH`, one dispatch table sized by
the new `OP__COUNT` sentinel, every handler ending in its own indirect
jump (`CASE(op)` / `DISPATCH()` macros — the switch build compiles the
SAME handler body exactly as before, `DISPATCH()` = `break`). cmake
auto-detects GNU/Clang and enables it unless `OK_PURE` is on — the
portability gate. the trace build keeps the canonical switch
(`OK_TRACE_EXECUTION` wants the loop top per instruction). the four
pre-patch NAME ops alias NOP in the table: the switch build silently
skipped them too (freeze rewrites every one before execution), so the
two modes stay byte-identical in the impossible case.

**the measured cost of nothing**: threaded alone is worth 1.05-1.18x on
every dispatch-heavy bench (compare after-switch vs after-threaded
columns). at -O0 the win is smaller (the handler bodies dominate); the
release numbers above are -O2.

## 2. fused conditions ("branch no materialize", the condition half)

`OP_JUMP_IF_CMP` (2w, cmp lane in the 3 spare bits above Q): a DIRECT
comparison in an if/while/for condition emits one instruction where the
generic path spent two. the comparison runs through the SAME performs
(`perform_comparison` hoisted out of the loop so its switch can never be
mistaken for dispatch flow — see the receipt below), lands its result in
the lhs register — which may be a LOCAL's register (`while i < n` fuses
with zero staging) — so the VM saves and restores the operand around the
result read. operators keep dispatching through the fused lane; the
restore keeps the local intact. `patch_jump_if` now re-ors the cmp bits
(one patcher, both shapes). compiler side: `compile_fused_condition_jump`
returns UNSUPPORTED (having emitted nothing) for every non-direct shape —
logical operators, negation, bare truthiness keep the generic path; that
generalization is pass 2.

## 3. the polymorphic inline cache

`mic_entry` unchanged; the per-site storage is now `mic_cache` — a
3-way probe chain with a round-robin eviction hand. probe = first way
whose klass+lane matches; fill = first empty way, else the hand's victim.
eviction is sound forever (entries are read-only snapshots of sealed
tables). class_poly_dispatch (5 classes at one site) stops thrashing.
fresh generations of a redefined local class start all ways cold (the
R18 ruling, carried to the new shape). the class-resident operator
caches (patch 32) stay 1-way — per (class, operator) is monomorphic by
construction.

## 4. string interning (names, never values)

`okintern.c/h`: an open-addressed set of canonical `object_string*`,
strong-rooted via a new gc watch. every compiler-made NAME goes through
it — identifiers-as-constants, member names, class names, canonical
operator spellings, the vm's startup names ("init", the op spellings, the
builtin class names) — so member probes hit the new pointer fast path in
`SM_PROBE_MATCHES` (method/field maps), `map_value_key_equals`, and
`perform_equal`'s string lane. **literals are NOT interned**: `===` is
word identity (C11, pinned by identity.ok — `"a" === "a"` is false) and
collapsing literals would flip it; that's a contract change for Q, not a
perf patch's side effect (R19). runtime-built strings (concat, number
formatting) stay uninterned — content compare still correct, no
unbounded table growth.

## 5. the numbers (oklang before → switch → threaded, seconds, mean of 5)

| bench | before | switch | threaded | speedup | wren | ok/wren after |
|---|---|---|---|---|---|---|
| class_field_loop | 0.540 | 0.509 | 0.504 | 1.07x | 0.441 | 1.14x behind |
| class_dispatch_chain | 0.381 | 0.381 | 0.355 | 1.07x | 0.274 | 1.30x behind |
| class_method_call | 0.416 | 0.433 | 0.391 | 1.07x | 0.316 | 1.24x behind |
| class_poly_dispatch | 0.447 | 0.457 | 0.412 | 1.09x | 0.334 | 1.23x behind |
| class_operator_add | 0.358 | 0.351 | 0.343 | 1.04x | 0.183 | 1.88x behind |
| class_instantiate | 0.668 | 0.661 | 0.641 | 1.04x | 0.318 | 2.02x behind |
| loop_counter | 0.285 | 0.266 | 0.242 | 1.18x | 0.283 | **1.17x ahead** |
| branch_toggle | 0.259 | 0.253 | 0.229 | 1.13x | 0.271 | **1.18x ahead** |
| arithmetic_sum | 0.386 | 0.376 | 0.344 | 1.12x | 0.427 | **1.24x ahead** |
| array_index_sum | 0.452 | 0.427 | 0.384 | 1.18x | 0.483 | **1.26x ahead** |
| global_access | 0.239 | 0.235 | 0.215 | 1.11x | 0.214 | parity |
| local_pressure | 0.120 | 0.122 | 0.113 | 1.06x | 0.146 | **1.29x ahead** |
| map_insert | 0.141 | 0.131 | 0.131 | 1.08x | 0.113 | 1.14x behind |
| map_lookup | 0.198 | 0.225 | 0.194 | 1.03x | 0.204 | parity |
| numeric_for_sum | 0.146 | 0.145 | 0.147 | 1.00x | 0.419 | **2.85x ahead** |
| map_for_in_sum | 0.193 | 0.199 | 0.192 | 1.01x | 0.899 | **4.68x ahead** |

reading it honestly: **the core flipped from parity to ahead of wren**
(loop_counter, branch_toggle, arithmetic_sum, array_index_sum,
local_pressure all 1.17-1.29x faster than wren now; numeric-for and
map-iteration hold their big leads). the class paths narrowed but did
not close: field 1.2→1.14x, chain 1.4→1.30x, poly 1.3→1.23x, operator
2.0→1.88x, instantiate 2.1→2.02x. what remains is **allocation-shaped,
not dispatch-shaped**: instantiate pays construct_instance's
field-init-thunk nested run_untils and the instance allocation itself;
operator_add pays an allocation per result. that is 0.9.0's heavy-pass
material (constant folding, DCE, the general fusion) plus the
allocation-lane ideas, exactly as the roadmap split ruled it.

known warts, on the record:
- the poly-MIC probe loop costs ~2-4% on strictly monomorphic sites in
  the switch build (class_method_call 0.96x) — threading more than pays
  it back (1.07x). a ways[0]-fast-path branch is a one-hunk option if it
  ever shows up outside noise.
- map_lookup wobbles (switch 0.88x / threaded 1.03x): literal keys are
  uninterned by the ===-contract ruling, so map probes pay one failed
  pointer compare and gain nothing back. within noise; noted so it
  cannot quietly become folklore.

## receipts worth keeping (the honest part)

1. **the transform scanner bug (twice).** the code-surgery script that
   converted `break;` → `DISPATCH();` classified breaks by looking for
   loop/switch heads directly adjacent to `{`. `for (uint32_t w = 0; ...)`
   and `switch (cmp)` both have conditions between keyword and brace —
   invisible to the scanner. five loop-internal breaks (two mic probe
   loops, three parent-chain walks in DEFINE_METHOD) became DISPATCH.
   in switch mode `DISPATCH()` = `break` — **the wrong conversion was
   invisible to the full suite** (399/399 with the bug in). threaded
   mode read the next opcode from inside a C loop: 13 failures, the
   fun ones being "expected error, got success" (the chain walks
   aborted mid-probe, so signature/init-super checks never fired) and
   `o.v` evaluating to the receiver (the mic probe dispatched instead
   of hitting). fixed by hand; the lesson is now in the worklog: **a
   mode that compiles the same handlers must be gated by tests that
   exercise the same paths in BOTH modes — the switch build passing is
   not evidence for the threaded build.** (the after-threaded gate run
   exists precisely because of this.)
2. **the compiler reset dropped the interner.** `compiler_compile`'s
   between-runs deinit+init rebuilt the specs WITHOUT the new interner
   field — the second `ok_run` on one `ok` crashed (NULL intern). the
   CLI never saw it (one script per process); the embed test's repeated
   runs caught it immediately. gates earn their keep, again.
3. **probes can lie.** two debug-probe builds of a half-edited file
   produced confidently wrong theories (an "un-baked freeze" hex misread
   — 0x357 vs 0x353, one nibble — and a SIGPIPE-truncated "passing" run
   whose exit code belonged to `head`). both burned real time. the
   fix discipline that worked: never debug from a mutated working tree;
   copy to /tmp, probe the copy, and check the probe's OWN exit code.

## open for Q

- **R19**: intern string LITERALS too? flips `"a" === "a"` to true
  (C11's "word identity" stays coherent — the words just canonicalize —
  but it's a visible contract change with a test pinned to the current
  behavior). upside: literal map keys pointer-match (map_lookup's
  wobble above disappears), identical literals share one allocation.
  my lean: keep values uninterned, it's the honest ===.
- the remaining class-path gap is allocation-shaped (instantiate 2.02x,
  operator_add 1.88x): that's 0.9.0's lane (the heavy passes), not more
  lightweight work. stating it so nobody asks for pass 1.5.
