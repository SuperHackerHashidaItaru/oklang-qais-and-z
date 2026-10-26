# spec: compile pipeline isolation (target: 26/10)

two bugs the user flagged, one fix shape. today the compiler writes into the
LIVE stores as it goes — a failed TU leaves half-defined globals and
half-compiled functions behind (hot-reload in the game = live corruption);
and the VM holds `source` hostage for its whole lifetime (only one file per
VM, ever).

## the invariant we want

> a compile either fully lands, or the VM state is byte-identical to before.

and: after compile, the source buffer belongs to nobody.

## design

split compile into three phases with explicit handoffs:

```
parse     -> ast (owns nothing shared)
compile   -> OkCompileResult {       // staging, fully private:
              constants, functions/protos, upvalue descriptors,
              global_descriptors_delta,   // names+flags ADDED this TU
              line tables, interned paths
            }
commit    -> atomically append into the VM's stores:
              globals: append descriptors, THEN values (never half-visible),
              functions: protos enter the object store before any value
              referencing them becomes reachable
```

- commit order matters for the GC: object_store gains protos first (they
  become sweepable but unreachable), then global values publish closures.
  with the current mark-from-roots gc this is safe without a pause as long
  as no ok code runs between the two steps — commit runs on the C side,
  single-threaded, so it already does.
- on ANY failure in compile: deinit the staging result. stores untouched.
  the error path is the same one that prints file:line today.
- `ok_run` becomes: parse -> compile -> **source_deinit** -> commit -> run.
  the VM keeps only: the interned path string (for error prefixes — already
  separate from the buffer) and per-function line tables (already in chunk
  source_info, already working — this round's stack traces are the proof).
- globals define/get on the C API (ok_global_define) already goes through
  globals_store_add — that path becomes "commit one global", reusing the
  same primitive.

## what this unblocks
- multi-file / modules: N TUs, sequential commits, one VM. module registry
  keyed by interned path. (post-halloween)
- hot reload in dorg3: recompile dorg3.ok at runtime; failure = old program
  keeps running untouched, success = swap. the game's manifest check is
  already begging for this.
- okc: the same OkCompileResult serializes to a .okb file; the runtime
  loader deserializes straight into a commit. compiler-in-a-separate-exe
  falls out of this design for free rather than being a separate feature.

## risks / notes
- the compiler's scope tables and hashed_strings currently allocate from the
  shared allocator with the gc watching (mark_compiler walks contexts). the
  staging result must be gc-rooted during compile exactly like today —
  mark_compiler already roots contexts, extend it to root the staging
  constants array. mind the lesson of this round: anything alive only in a
  temp/staging slot must be inside a rooted window at every allocation.
- ok_call from C between a failed compile and the next compile must see the
  OLD world. test: compile bad TU, call a global defined before it, expect
  old behavior (exit code + output identical to never having compiled).

## tests
- bad TU (syntax error mid-file after valid glob fu + glob let) leaves
  zero new globals; previous program still runs; run twice in a row.
- good TU twice in one VM (redefine): v1 semantics = second commit REPLACES
  functions wholesale and resets the new globals' values to null; document
  loudly. (value-preserving reload is a v2 decision.)
- source buffer freed: run 100 files through one VM, RSS stays flat (leak
  check under asan).
