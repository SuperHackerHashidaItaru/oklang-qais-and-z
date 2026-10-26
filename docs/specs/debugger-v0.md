# spec: debugger v0 (written against the current impl, 24/10)

the rewrite starts now per user decision. this spec is grounded in what
exists TODAY after the boot-fix round — the line-info machinery below is not
planned, it is running in dorg3's error output right now.

## what exists today (verified this round)

- per-function line tables: chunk.source_info — run-length encoded
  (ok_line_info_repeated{reps, {line, offset}}), written by
  chunk_write_word/2words at every emit. chunk_find_line_info does
  ip -> line+offset. used by runtime_error and the stack trace.
- stack traces: runtime_error walks exe->call_stack printing
  `path:line:offset in name.` per frame. this round proved it works under
  real game code (dorg3.ok traces are correct, including named frames).
- function names on every object_function + closure debug print.
- okdebug.c: a raw-word disassembler placeholder (width-correct, format not
  final). okchunk.h contract comments are the authoritative encoding doc.
- OK_TRACE_EXECUTION (defined by OK_PARANOID): per-instruction register
  window dump + disassembly — the trace loop that will become the
  debugger's step mode.
- exe->native_call_depth: distinguishes "in ok code" from "inside a native"
  — needed for step-into vs step-over semantics.

## what v0 adds (post-halloween, ~1 day of work)

1. **breakpoint opcode**: OP_BRK, 1 word, no operands. compile-time: the
   debug build can patch any instruction's first word to OP_BRK (saving the
   original) — no recompile needed, `--break file:line` maps via the line
   table to an ip, patches in place. VM on OP_BRK: if debugger attached ->
   invoke the debug callback (vm owns a `debug_hook(vm*, debug_event*)`);
   else NOP (zero cost when unused — the p bit can gate even the branch).
2. **debug_event**: {kind: BREAK|STEP|WATCHPOINT, frame snapshot, ip, line}.
   the hook receives the exe pointer: everything inspectable (registers,
   call stack, closures) is already reachable from it — no new state
   structures needed.
3. **stepping**: STEP_OVER = run until call_stack.count returns to entry
   depth; STEP_INTO = one instruction (reuse the OK_TRACE_EXECUTION loop
   shape); STEP_OUT = run until count < entry depth. all three are just
   run_until(depth) variants — the primitive already exists and is battle-
   tested by vm_call.
4. **inspection API** (C, feeds any frontend):
   - ok_dbg_frames(ok*) -> [{name, path, line, ip}]
   - ok_dbg_locals(ok*, frame) -> [{name, value}] — needs local name
     tables: chunk.identifiers exists but is not yet slot-mapped per ip
     range. v0 compromise: function-level name->slot map (names recorded in
     add_local order; correct for non-shadowed scopes, documented wrong
     when shadowing). v1: per-ip-range slot maps in source_info.
   - ok_dbg_globals(ok*) -> walk globals_store + descriptor names.
5. **transport v0**: `ok --debug script.ok` = line-oriented REPL on stdin
   (b <line>|c|so|si|so out|p <expr>|bt|q). the game embeds the same hooks
   via a C callback — dorg3 gets breakpoints in-process for free.
   JSON-lines/DAP-subset later; the hook design makes the frontend dumb.

## non-goals v0
watchpoints (needs write barriers), conditional breakpoints beyond `if`
expressions evaluated by the REPL calling back into the VM (careful:
re-entrancy through ok_call is safe per this round, but errors inside the
eval must not clobber the paused exe — the staging/commit isolation spec
covers making eval side-effect-free-ish), remote protocols.

## sequencing
the two things v0 hard-depends on land THIS week anyway: line tables
(done), pipeline isolation (26/10 — gives eval-without-corruption). so the
debugger itself is purely additive post-halloween work.
