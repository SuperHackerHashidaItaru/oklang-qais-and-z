# patches — pending

deliverables awaiting Q's review + hand-application, newest last. apply in
order (each patch is stacked on the previous):

1. `07-floor-modulo.patch` — vs `442ad31` (master after 05+06). your ruling:
   floored modulo (python/lua), floor_mod helper + identical int48 arm
   adjustment, tests updated (`-7 % 3 == 2`, wraparound dogfood). e2e 203/203
   x plain/release/aggressive/asan/asan-aggr.
2. `08-github-ci.patch` — stacked on 07. `.github/workflows/ci.yml`: plain +
   asan on the GH mirror (free runners), unity via submodules, ctest + e2e +
   embedding, aggregated status cross-post to codeberg's statuses api
   (`ci/github-actions` context). needs the `CODEBERG_TOKEN` secret — setup
   notes in the guide addendum. first live run is the real test (no cmake in
   my sandbox).

3. `09-no-question-unbreak.patch` — vs `0a88601` (no-question branch tip,
   NOT master). unbreaks the half-finished rewrite: swapped string_deinit
   args (build), oppAe e-field mask/offset + bool-truncation (every return
   was null — 65/206 failures alone), hidden-scope loop revived + user
   top-level scope + `p_initialized` wiring (core names resolve, no
   `redefining local` at top level), `TOKEN_AS` → `OPERATOR_BIN_AS` mapping
   (`as` used to abort the compiler), as_bool missing else (`null as bool`
   → true), PREC_AS moved between EXPONENT and PREFIX (rust cast level;
   design.txt §1B requires it above SUM), opcode-name table order fix,
   disassembler `as` global-operand print (the branch's own known issue).
   e2e after: 210/211 — the 1 is the open `glob let len` semantics
   decision (REVIEW-GUIDE-round14.md §4, recommendation: keep
   locals-first + add a warning). plus new `operators/as/` tests (the
   operator had zero coverage).

4. `10-gc-guard-stack-and-okrun-rooting.patch` — stacked on 09. fixes the
   asan+aggr mass failure your gate caught ([39 OK], [167 NOTOK]): the
   branch's `ok_get_top_level_values` allocates between compile and
   vm_interpret (a fresh hashed key string per `globals_store_get`),
   aggressive gc swept the compiled function that was reachable only from
   a C local, and vm_interpret guarded a corpse → UAF in mark, or a
   recycled-zeroed chunk → SEGV on `ip == NULL` at the first fetch.
   master was safe only because it had no allocating step in that window.
   fix: ok_run guards the function across the gap + the guard slot became
   a 16-deep stack (retires vm_interpret's `TODO array based gc guard`;
   nested pairs compose, underflow-down is a no-op). also updates
   `shadow_len_does_not_invalidate_for_in.ok` to your ruling (for-in reads
   `' len'` directly; `len(arr)` still the builtin).
   e2e 211/211 x plain/asan/asan-aggr, verified again on a fresh
   0a88601+09+10+11 clone. full story: `POSTMORTEM-aggressive-gc.md`.
5. `11-as-operator-e2e-tests.patch` — the 5 `operators/as/` tests that 09's
   entry claimed but never actually shipped (your 206 vs my 211).
   conversion matrix + error cases; `as` had zero coverage before.
6. `12-gc-rooting-and-traced-natives.patch` — vs `bc272e3` (master after the
   no-question merge). the rooting api: `ok_native_tracer` callback +
   `ok_gc_mark_value` (trace-phase-gated via `gc.is_tracing`), `ok_root`/
   `ok_unroot` with a recycled-id freelist, roots marked in `mark()`,
   `trace_object` OBJ_NATIVE_VALUE case. **breaking**: `ok_native_value_new`
   takes a 4th tracer arg (dorg3 ports with `, nullptr`). also fixes the
   embed test's stale `?` syntax — it was red on master (§2 of the guide).
   embed test grows the string-bag traced-native + root-recycling sections.
7. `13-array-public-api.patch` — stacked on 12. `ok_array_new/get/set/push/
   len` + `ok_value_is_array`, with the OP_ARRAY_NEW guard pattern adapted
   to host-held (invisible) values. the fork-array workaround's replacement.
8. `14-print-hook.patch` — stacked on 13. `ok_set_print(ok*, cb, ctx)`: print
   routes the raw value to the host, CLI default byte-identical. the
   playground's stdout capture.
9. `15-ci-aggressive-lane.patch` — independent (yaml only). adds the
   aggressive-asan matrix entry to ci.yml — the lane that catches the
   167-NOTOK class.

   12–15 verified: e2e 211/211 x plain/asan/asan-aggr, embed test x3, fresh
   sequential apply on bc272e3 + rebuild + full gate. contracts and the
   unexposed-features audit: `REVIEW-GUIDE-round15.md`.

`REVIEW-GUIDE-night13.md` (+ its addendum) — the whole-night bulk-review
guide: per-patch where-to-look, the 06 proof obligations, the bench read
with its noise-floor analysis, the CI setup steps, open items.

after Q applies + pushes, these move to archive/patches/ and this dir empties.

## history

- rounds 10's arrays-v2 + default-args patches: APPLIED by Q (squash
  `9e9fd41`), archived under archive/patches/.
- rounds 11's numeric-for / disassembler / assign-peephole (01-03): APPLIED
  by Q (squash `55e1293`).
- round 12.5's buffer-night patch (04: ternary, string concat, equality fix,
  token purge, elision, window fix): APPLIED by Q (squash `4633dc9`, yolo).
- night-13's 05-modulo + 06-forloop-imm-fastpath: APPLIED by Q (`442ad31`,
  with `-lm` as `target_link_libraries(ok PUBLIC m)`).
