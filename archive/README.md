# archive/patches — the paper trail

every patch Z ever delivered, in the order the bugs were found. all of
these were reviewed and hand-applied by Q and pushed to codeberg — they
are history, not pending work. kept here so the round logs' references
resolve and so the debugging narratives stay auditable.

chronological order:

1. `oklang-register-fixes.patch` — the big one (~30 fixes, session 2):
   the patch set that took the register VM from first-execution to
   109/109 e2e. jump offsets/polarity, 2-word decode, resolve_local
   double indirection, upvalue indices, call convention, the works.
2. `PORT_README.md` — the full port narrative that shipped with it.
3. `gc-and-leak-fixes.patch` (session 3) — non-strict tag checks vs
   double payload aliasing, closure-at-top rooting window, register
   null-fill watermark, pop-without-deinit leaks, call_native frame
   assumption.
4. `dorg3-boot-fix.patch` (session 4) — GC watermark collapse in
   call_native + cleanup_frame (the "expected 8 arguments, got 0" bug)
   + error-reporting overhaul.
5. `load-null-fix.patch` (session 5) — OP_LOAD_NULL count-vs-end-index
   mismatch (the music bug), + regression test.
6. `vm-global-trace.patch` — optional -DOK_TRACE_GLOBALS instrumentation.
7. `native-callback-fixes.patch` (session 6) — vm_call double-base,
   error_pending propagation, GC stale-window, argv invalidation; the
   patch that made the full dorg3 game loop survive end-to-end.
