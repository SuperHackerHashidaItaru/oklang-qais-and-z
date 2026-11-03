# MODULES — the 0.6.0 wave draft (imports, native loading, okproject, embedding)

status: DRAFT for Q. written 2026-11-03 against the coro tree (origin/coro,
1672c21) plus this session's two landed changes (defer continuation, the
type/object rename — patches 39/40 in the collab repo). this is the big
inclusive draft Q asked for: "imports are compile time for most part, imports
use \"\" '' not plain like python, import should apply to both c-level modules
and raw oklang files, we should also draft a dynamic loader, to load native c
into oklang from the cli without embedding it... we need rules for api, and
project shape... okproject.ok at root, mirroring pyproject.toml but strictly
in ok... thus we have to make a big draft that is inclusive of all aspects."

the verdict i'm asking for is the MR rulings at the end (M1-M14) plus the
patch-series split. the mechanics are, as usual, mostly paid for by machinery
that exists — the receipts are in section 0.

## context restore (read this first in a fresh session)

- repos: codeberg.org/qais/oklang is the language (branches: master, then
  classes-staging, then coro built on it — Q applies patches by hand, the
  standing protocol). the collab repo (docs/patches/bench) mirrors on github
  (SuperHackerHashidaItaru/oklang-qais-and-z).
- sandbox: no cmake. build with /home/z/my-project/scripts/build_ok.sh
  [plain|threaded|release|release-threaded|asan|aggressive|...]; the gate is
  scripts/run_e2e_full.sh (expect: stdout lane + expect error: stderr lane)
  run per build flavor; 425/425 is green as of this draft.
- the e2e truth: tests live in oklang/tests/e2e/tests/<area>/*.ok. new
  features land with tests in the same commit, always.
- doc index: docs/permanent/{ROADMAP,DECISIONS,PROJECT_STATE}.md are the
  living truth; docs/drafts/ holds the feature drafts (CLASSES, COROS,
  MODULES=this); WORKLOG.md is the session log, newest at the bottom.
- the two changes that just landed (this session): defers continue past a
  failing defer (zig/go doctrine, first-failure-wins per chain, patch 39),
  and Class/Object renamed to type/object (patch 40).
- this wave's number is 0.6.0 (per WORKLOG: "then 0.6.0 (modules, with coro
  as a candidate first resident per R24)"). 0.9.0 after it is the heavy
  optimization closure (constant folding, general fusion, DCE-lite, jump
  threading, tailcalls) — the classes allocation gap owns that lane.

## 0. what already exists and pays for this

- **the C API is done** (patch 27, include/ok/ok.h): ok_run/ok_call,
  ok_global_define(+native), ok_native_value_new with tracer/destructor
  (GC-visible native values!), ok_root/unroot, print hooks, error lanes.
  a native module author needs ZERO new host API to write functions — the
  module layer is a packaging/loading story on top, not a new ABI.
- **natives-as-classes exists** (patch 34): ok_class_new +
  ok_class_define_native_method + ok_class_freeze. a C module can ship real
  classes (the `coro` builtin is the proof-of-concept resident). this is the
  door raylib walks through later: a C library presented as oklang classes.
- **compile-time globals** (D2): names resolve at compile time to indices.
  module exports are the same trick one level up — `io.print` compiles to a
  slot load, zero runtime name lookup. the doctrine generalizes cleanly.
- **the snapshot-swap pipeline** (D8): the compiler already compiles against
  a snapshot of global state and commits atomically. "compiling a module" is
  "compiling a TU against a snapshot" — D9's line, still true.
- **the interner** (patch 36): module registry keys are interned paths, the
  same machinery that made member probes pointer-fast.
- **the e2e matrix**: multi-file tests need a tiny runner extension (a test
  dir with imports + expect files) — the harness is otherwise ready.
- **wren's module system** as the cautionary receipt: wren imports are
  runtime-executed strings with no project shape; fine for a scripting
  embedder, insufficient for "the language is the whole tool". we take the
  compile-time lane instead and never look back.

## 1. the goal (Q's framing, recorded so it survives)

the language becomes **fully usable standalone**: write ok files, import ok
files, import native C modules, build what needs building — all from the
oklang CLI, no embedding shells, no wrapper apps. the explicit future doors
this opens (Q's words): port raylib to it, port emscripten loop APIs
(emscripten's main-loop model), "make stuff directly from oklang from oklang
cli without touching either native or creating a wrapper app" — the shape
the python ecosystem diverged into (language + packaging + native exts),
done small and zero-dep. casio/asio stays a separate later project
(standing: no scheduler in this wave).

## 2. the surface

```ok
// three forms, all compile-time (M1-M4):
import "std/io";                      // load + init; binds `io` (the stem, M5)
import "std/math" as m;               // explicit alias
from "std/str" import upper, split;   // bind symbols directly

// the same surface imports natives — no separate syntax (M2):
import "raylib";
from "raylib" import window, draw_text;

// file-relative (raw ok files, mixing allowed — M6, M7):
import "./util.ok";
from "../lib/helpers" import tick;

// module member access — compiles to a slot load (D9):
io.print("hi");
m.floor(x);

// the explicitly-dynamic escape hatch (D9, P8, can slip past 0.6.0):
let plug = import(plugin_name);       // runtime name -> module VALUE
plug.call_me();                       // name-based dynamic access
```

- paths are **quoted strings, single or double** (M1) — same literal rules
  as string literals, `import std.io` python-plain is NOT the language.
  this supersedes D9/ROADMAP v2's `import x` spelling (their semantics
  carry over; only the spelling changes).
- `from "x" import a, b` — symbols must be `pub` in x. `import *` is
  rejected (explicitness doctrine, M8).
- no `__main__` magic (D9): the entry file is a plain module whose top level
  runs after its imports. `is_main()` only if a real case appears.
- what is an export: **`pub` at module top level** (M9). `pub fu`, `pub
  let`, `pub class`, `pub static` — the modifier already exists and already
  means "visible outside"; modules give it a second, outer ring. everything
  else is module-private. no separate export list syntax to design.

## 3. the semantics (compile-time, zero runtime cost — D9 carried whole)

- the compiler resolves the import graph depth-first at compile time. each
  module = one TU compiled once, keyed by RESOLVED absolute path (interned).
- a module's exports are a **slot array + name table** (the globals trick
  one level up). `io.print` after `import "std/io"` compiles to: module
  value load + slot load. no runtime lookup, ever, for the static lane.
- **top-level code runs once**, on first import, before the importer's
  top level. the registry caches the initialized module value. init order
  is the depth-first order; a cycle is a hard compile error in v1 (D9's
  hard-fail doctrine — "the cycle is in YOUR design, fix the design").
- the module value itself: a frozen object with the export slots (one
  shape per module, built at freeze time — same machinery as class
  shapes). it is an ordinary value: store it, pass it, put it in a map.
- shadowing: an import binding is a scoped binding like any `let` — inner
  scopes may shadow it; redeclaring two imports under one name in the same
  scope is the usual duplicate-binding compile error.
- natives: same graph, same forms. the difference is entirely in the
  LOADER (section 5): `"raylib"` resolves to a shared object, the module
  value is built from its descriptor instead of a compiled TU. `from
  "raylib" import draw_text` binds the native directly — the static lane
  does not care that the export is a C function.
- the dynamic lane (`import(name)`, a runtime call): returns the module as
  a value; all access through it is name-based (the future dynamic-globals
  path + inline caching). two worlds chosen by syntax at the use site
  (D9's exact shape). the usual case stays the fast case.

## 4. okproject.ok — the project shape (M10)

a file at the project root, **strictly in ok** (it runs through the real
parser + VM — no second config format to maintain), mirroring pyproject.toml
in role. the CLI evaluates it in a restricted ok instance (a fresh ok with
NO natives registered — no io, no fs; the file must be pure data) and reads
one global:

```ok
// okproject.ok
glob let project = {
  "name": "myapp",
  "version": "0.1.0",
  "ok": "0.6",             // the oklang surface this project targets

  // where the project's own ok modules live (import roots, relative to
  // this file). default ["src"] when absent.
  "modules": ["src"],

  // path deps only in v1 (M11): no registry, no downloads, no version
  // solving. a dep is a reachable directory: another okproject, or a raw
  // directory of ok files, or a native module dir. mixing is fine.
  "deps": {
    "mathx":   {"path": "../mathx"},           // another okproject
    "helpers": {"path": "vendor/helpers"},     // raw ok files, no project
    "raylib":  {"path": "native/raylib"},      // has natives (below)
  },

  // native build targets (M12): each becomes an importable module name.
  // "cmake" is optional — a prebuilt .so may be dropped in "bin".
  "natives": {
    "raylib": {
      "sources": "native/raylib",
      "cmake": "native/raylib/CMakeLists.txt",
    },
  },
};
```

rules:
- discovery walks UP from the importing file to the nearest okproject.ok;
  that root governs. a raw ok file with no project anywhere above it is a
  standalone script: file-relative imports only (plus std, plus OKPATH).
- a dep that is itself an okproject recurses: its own roots/deps/natives
  apply. dep names must be unique in the import path namespace (M13:
  collision = error naming both paths).
- **no pip, ever, in this wave** (M11): a module is something reachable on
  disk. the okproject says where. this is deliberate — the registry
  question (if any) is a post-1.0 conversation with completely different
  tradeoffs, and this design does not preclude it.

## 5. the loader — one door, three backs (M3, M14)

the VM and the compiler NEVER touch the filesystem for imports. all loading
goes through a vtable the host installs:

```c
// include/ok/ok_module.h (new, P4)
#define OK_NATIVE_ABI 1

typedef ok_value (*ok_native_fn)(ok* p_ok, uint8_t p_argc, const ok_value* p_argv); // same as ok.h

typedef struct ok_native_fn_def {
  const char* name;
  uint8_t arity;              // counts the receiver where applicable; OK_NATIVE_ARITY_ANY allowed
  ok_native_fn call;
} ok_native_fn_def;

typedef struct ok_native_module {
  uint32_t abi;               // must equal OK_NATIVE_ABI, checked at load
  const char* name;           // the module's self-declared name
  const char* version;
  uint32_t fn_count;
  const ok_native_fn_def* fns;
  // optional class surface: built at load via the patch-34 door
  // (ok_class_new/define_native_method/freeze). NULL = none.
  bool (*mount_classes)(ok* p_ok);   // returns false on failure (error lane set)
  // optional lifecycle. on_load runs at first import; on_unload at ok_free.
  bool (*on_load)(ok* p_ok);
  void (*on_unload)(ok* p_ok);
} ok_native_module;

// the ONE symbol the shared object must export:
// OK_API const ok_native_module* ok_native_export(void);

// the loader vtable (host installs; the CLI installs the fs default)
typedef struct ok_loader {
  bool (*read_source)(void* p_ctx, const char* p_path, ok_source* p_out);      // fills code+path (borrowed ok_source rules)
  bool (*load_native)(void* p_ctx, const char* p_path, const ok_native_module** p_out); // dlopen + dlsym + abi check
  void* p_ctx;
} ok_loader;
void ok_set_loader(ok* p_ok, ok_loader* p_loader);
```

why a vtable and not "just read files":
- **the embedding working-dir problem, killed by construction** (Q: "when
  embedding, and want to import, we dont know what the embedders working
  dir is"). paths resolve against EXPLICIT roots (the loader's), never
  against an implicit cwd. the CLI's default loader resolves cwd +
  okproject discovery + OKPATH; an embedder (the engine, a game, an
  emscripten build) installs its own: memory-backed sources, packaged
  assets, wasm fetches. the language never guesses.
- **emscripten/wasm is a backend, not a redesign**: load_native there is a
  static registration table (no dlopen on wasm) — the vtable is the seam.
- testability: the e2e matrix gets a memory-backed loader test in C (P7),
  which is also the embedder documentation by example.

loading discipline for natives: dlopen(path, RTLD_LOCAL) — a module's
symbols never leak into another's namespace. dlsym("ok_native_export"),
abi check first, name check second (the path's stem and the declared name
must agree for graph imports; force-loads register under the declared
name). a module is loaded ONCE per ok instance (the registry). on_load
failures and abi mismatches produce one clear error: `module 'raylib':
abi 0, expected 1 — rebuild against include/ok/ok_module.h`.

## 6. resolution (the search order, M6)

for an import path P from file F:

1. P starts with `./` or `../` (or is absolute): resolve against F's
   directory. exact file, then P + ".ok", then P + "/index.ok". raw ok
   files, no project needed. this is the mixing lane.
2. P starts with `std/`: the standard library — the std/ directory that
   ships beside the ok binary (or compiled-in per embedder choice). never
   found through steps 3-5: user code cannot shadow std (M8).
3. walk up from F to the nearest okproject.ok root R: search R's module
   roots (["src"] default) for P (same file/`.ok`/`index.ok` ladder).
4. R's deps: for each dep D named P or exposing P under its own roots —
   recurse into D's project. native targets match by target name (build
   first if stale — section 7).
5. OKPATH (colon-separated) / embedder-provided extra roots. last, always.

- a miss prints the whole trace: `cannot import "mathx": searched
  ./..., src/..., deps(2), OKPATH — not found. (ok where "mathx" prints
  the same without running)`. the 404 must teach.
- an ambiguity (two deps expose the same name) is an error naming both
  paths (M13). the fix is always explicit: `import "./vendor/x"` or an
  `as` alias can't help here — rename or path-import one of them.

## 7. the CLI (M3, M12)

```
ok run app.ok                 # unchanged shape; imports resolve per section 6
ok run --native m.so app.ok   # force-load a native module outside any project
                              # (registers under its declared name BEFORE compile)
ok build                      # build native targets (cmake), refresh .okbuild/
ok build --clean              # nuke .okbuild/ and rebuild
ok where "mathx"              # print the resolution trace, exit 0/1
```

- `ok build` drives cmake per native target (the okproject declares the
  CMakeLists), output cached under `.okbuild/<target>/<hash>/` where the
  hash covers the target's sources + include/ok headers — stale = rebuild,
  fresh = reuse. `ok run` auto-builds stale native deps only when a project
  is present (a bare script never builds anything — scripts may only
  `--native` force-load or import prebuilt/OKPATH modules).
- cmake templates ship in the repo (`project template/` someday; for now a
  documented CMakeLists snippet: one shared object, includes oklang's
  include/, exports ok_native_export). the sandbox has no cmake — P6 lands
  with a plain-gcc fallback path (`"build": "gcc -shared ..."` allowed as
  an okproject alternative — M12 sub-ruling) so the e2e matrix still runs.
- `--native` force-load is the "truly general purpose" door Q asked for:
  load ANY conformant shared object into oklang from the CLI, no embedding
  shell, no wrapper app.

## 8. the patch series (one session each, landable in order)

each patch: green e2e matrix (4 build flavors) + new tests + WORKLOG entry.
if a session dies mid-patch, the next session reads this section + WORKLOG
and resumes — that is the whole point of the split.

- **P1 — the grammar** (~150 lines + tree-sitter): `import`/`from`/
  `as`/symbol lists with quoted paths; hard error on bare identifiers
  after import ("quote the path"). AST nodes, parse tests, tree-sitter
  grammar + corpus. no resolution, no semantics. acceptance: everything
  that parses today still parses; import forms parse and pretty-print.
- **P2 — the module graph + ok-file imports** (the big one, ~450 lines,
  split into P2a/P2b if the session runs long): loader vtable + the fs
  default; path normalization + the resolution ladder; cycle detection;
  TU-per-module compile against the snapshot (D8); the module registry
  (interned resolved-path keys); the module VALUE (frozen shape + export
  slots + name table); `pub` = export; member access compiles to slot
  load; run-once init in depth-first order; `import` binds stem/alias,
  `from...import` binds symbols. acceptance: multi-file e2e (import
  chains, cycles error loudly, init order, pub/privacy, shadowing).
- **P3 — stdlib re-home**: std/ directory (io, math, str, array, map, fs,
  time, coro — coro becomes std.coro per R24's always-loaded ruling
  relaxed to "std module, preloaded by default for now"); natives
  re-registered as module members; flat globals stay as deprecated
  aliases (removal in 0.7.0, M14 sub-ruling). acceptance: old scripts
  unchanged (aliases), new-style scripts pass, docs table of std modules.
- **P4 — native modules + dlopen + `ok run --native`** (~300 lines):
  ok_module.h, the descriptor, the loader's load_native (RTLD_LOCAL, abi
  check, name check), registration under the module namespace, class
  mounting through the patch-34 door, lifecycle hooks. a hello-native
  sample module (one function + one class) + e2e with a prebuilt .so.
  acceptance: `from "hello" import greet; greet("x")` works end to end;
  abi-mismatch and bad-symbol errors are the documented messages.
- **P5 — okproject.ok** (~300 lines): discovery, the restricted evaluator
  (no-natives ok instance), module roots, deps (path), the resolution
  order of section 6 wired to real directories, ambiguity/missing errors
  with traces, `ok where`. acceptance: the fixture farm (project with
  deps-on-project, raw-dir dep, missing dep, ambiguous dep, nested
  projects).
- **P6 — `ok build`** (~200 lines + templates): cmake driving, the
  .okbuild staleness cache, build-on-import for stale native deps, the
  gcc-shared fallback, `ok build --clean`. acceptance: a native dep
  builds once, is reused, rebuilds on touch; scripts never build.
- **P7 — embedder loader API + docs** (~150 lines): ok_set_loader public,
  the memory-backed loader C test (which is the embedder example), the
  no-implicit-cwd doctrine written into ok.h docs, the emscripten
  registration-table note. acceptance: e2e runs a whole import graph from
  memory with zero fs calls.
- **P8 — dynamic import** (stretch, may slip past 0.6.0): `import(name)`
  runtime call + name-based module access with inline caching (D9's
  dynamic lane; shares machinery with the future dynamic-globals path).

order rationale: grammar first (unblocks tree-sitter + editor tooling
immediately), ok-files before natives (the module VALUE and registry are
the shared spine — natives are just another producer of one), stdlib
re-home right after ok-files so P4+ can import std natively, okproject
after the loader mechanics exist (it is policy over mechanics), build
last-but-one because it is pure CLI plumbing, dynamic import last because
it is the only non-compile-time lane and the least load-bearing.

## 9. what this wave deliberately does NOT do

- no scheduler, no async, no casio/asio (standing veto; separate project).
- no package registry, no downloads, no version solving (M11). deps are
  paths. the day a registry is wanted, okproject grows a "source" field —
  nothing here precludes it.
- no hot reload (pipeline isolation v2's question).
- no per-module GC isolation, no module unloading at runtime (on_unload
  runs at ok_free; a dead module stays loaded — the lua lesson).
- no conditional compilation / imports (the preprocessor is a lie).
- no star imports (M8).

## 10. the rulings i need (M1-M14)

- **M1**: import paths are quoted ("..." or '...'), python-plain `import
  x` is not the language. supersedes the D9/ROADMAP spelling.
- **M2**: one import surface for ok files and native modules; the resolver
  decides, the syntax never distinguishes.
- **M3**: the CLI loads native modules directly (`ok run` via okproject
  rules, `ok run --native` for force-loads) — no embedding shell needed.
- **M4**: imports are compile-time; the ONLY runtime lane is `import(name)`
  (P8, optional for 0.6.0).
- **M5**: `import "a/b"` binds the stem `b` by default; `as` overrides.
  (alternative: no default binding — bare `import "x"` becomes a
  load-and-init-only statement. my lean: stem-binding, python-warm and
  wren-cold both accommodated.)
- **M6**: resolution order = file-relative, std/, project roots, deps,
  OKPATH — in that order, with the full trace on a miss.
- **M7**: raw ok files, project-less dirs, and okprojects mix freely (a
  dep may be any of the three).
- **M8**: std/ is unshadowable; `import *` rejected.
- **M9**: `pub` at module top level is the export rule; no new syntax.
- **M10**: okproject.ok, strictly ok, evaluated in a no-natives ok
  instance, reads one `project` map.
- **M11**: path deps only in v1 — no registry, no downloads, no version
  solving.
- **M12**: native targets build via cmake per okproject (with a plain-gcc
  fallback declared in okproject); `.okbuild/` cache; scripts never build.
- **M13**: dep name collisions are errors naming both paths.
- **M14**: the loader vtable is the ONLY way the compiler/VM see the fs;
  flat stdlib globals remain as deprecated aliases until 0.7.0.

plus one sub-ruling worth stating out loud: **coro's home becomes
`std.coro`** in P3 (R24's "always loaded for now" was explicitly awaiting
this), with the flat `coro` binding kept as the deprecated alias.

## 11. the future doors this opens (recorded, not scheduled)

- **raylib as an oklang module**: ok_class_new mounts real classes with
  native methods; raylib's C API is mostly functions + a few structs —
  a mechanical binding generator over ok_module.h is a weekend, and then
  oklang writes games with no shell. the first real dogfood of everything
  above.
- **emscripten loop APIs via import**: the wasm loader backend +
  `import asio` (casio, later) gives the browser main-loop model natively
  — the web fps wobble fix and the "port some emscripten apis" wish ride
  the same seam.
- **the static-site / portfolio pledge**: fs_ + modules = the static-site
  generator in pure oklang (D16's fs natives + P3's std.io/std.fs).
- delta blue (this session's other delivery) is the 0.9.0 heavy-pass
  meter: modules land first, then the allocation-shaped gap closes, then
  the language is genuinely in the race.
