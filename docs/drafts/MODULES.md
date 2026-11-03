# MODULES — the 0.8.0 wave, codename "O Resolution" (final)

status: FINAL. the M1-M14 rulings came back (2026-11-03, recorded verbatim
in section 11), the sandboxing gap Q caught is designed in (section 6), the
patch series is re-densed per Q's "the plan was too sparse" (4 patches, was
8 - section 10). the pre-modules work Q ordered landed first: string
interning is patch 41 (D20, 427/427 x5) and the coro C API is specced as
patch 42 (appendix A) so ok_module.h's vtable inherits it instead of
breaking later. OR-1 can start.

renumbering note: this wave was drafted as 0.6.0; Q's tidy lane (classes
fix + defer continuation + the rename + interning + the coro C API) is
v0.7.0 now, so modules is **0.8.0**. one stale in-tree reference (okvm.h's
"the 0.7.0 registry" comment) is OR-1 scope. the codename is Q's pick: "O
Resolution" (wilderun, veil of imagination - we are, after all, resolving
paths).

## context restore (read this first in a fresh session)

- repos: codeberg.org/qais/oklang is the language (Q applies patches by
  hand, the standing protocol; branches: master, classes-staging, coro,
  and Q's tidy = coro + patches 39/40). the collab repo (docs/patches/
  bench) mirrors on github (SuperHackerHashidaItaru/oklang-qais-and-z).
- sandbox: no cmake. build with /home/z/my-project/scripts/build_ok.sh
  [plain|threaded|release|asan|aggressive|...]; the gate is
  scripts/run_e2e_full.sh (expect: stdout lane + expect error: stderr
  lane) run per build flavor; 427/427 is green as of patch 41.
- the e2e truth: tests live in oklang/tests/e2e/tests/<area>/*.ok. new
  features land with tests in the same commit, always.
- doc index: docs/permanent/{ROADMAP,DECISIONS,PROJECT_STATE}.md are the
  living truth; docs/drafts/ holds the feature drafts (CLASSES, COROS,
  MODULES=this); WORKLOG.md is the session log, newest at the bottom.
- the 0.7.0 lane (Q's tidy branch): defer continuation (D17, patch 39),
  the type/object rename (D18, patch 40), universal string interning
  (D20, patch 41), and the coro C API (appendix A, patch 42 proposal).
  this wave stacks on top of all of it.
- delta blue (session 33) is the 0.9.0 meter: ok at 1.93x of wren on the
  allocation-shaped gap. modules first, then the heavy passes.

## 0. what already exists and pays for this

- **the C API is done** (patch 27, include/ok/ok.h): ok_run/ok_call,
  ok_global_define(+native), ok_native_value_new with tracer/destructor
  (GC-visible native values!), ok_root/unroot, print hooks, error lanes.
- **natives-as-classes exists** (patch 34): ok_class_new +
  ok_class_define_native_method + ok_class_freeze. a C module can ship
  real classes (the `coro` builtin is the proof-of-concept resident).
  this is the door raylib walks through later.
- **compile-time globals** (D2): names resolve at compile time to indices.
  module exports are the same trick one level up - `io.print` compiles to
  a slot load, zero runtime name lookup.
- **the snapshot-swap pipeline** (D8): the compiler already compiles
  against a snapshot of global state and commits atomically. "compiling a
  module" is "compiling a TU against a snapshot" - D9's line, still true.
- **the interner is universal now** (D20, patch 41): every ok string is
  canonical, and the table is the module registry's key source by design
  (interned, deduped paths). 'a' === 'a' closed the C11 question on the
  way.
- **the e2e matrix**: multi-file tests need a runner extension (a test
  dir with imports + expect files) - the harness is otherwise ready.
- **wren's module system** as the cautionary receipt: runtime-executed
  import strings, no project shape. we take the compile-time lane.

## 1. the goal (Q's framing, recorded so it survives)

the language becomes **fully usable standalone**: write ok files, import ok
files, import native C modules, build what needs building - all from the
oklang CLI, no embedding shells, no wrapper apps. the explicit doors this
opens (Q's words): port raylib to it, port emscripten loop APIs, "make
stuff directly from oklang from oklang cli without touching either native
or creating a wrapper app". casio/asio stays a separate later project
(standing: no scheduler in this wave). and the embedding-side duty that
comes with the power: sandboxing, from the same moment loading exists -
"oklang shouldn't be the escape hatch that allows to load arbitrary
native code" (Q).

## 2. the surface

```ok
// three forms, all compile-time, all quoted (M1, M2, M4):
import "std/io";                      // binds the PATH: use std.io.print(...)
import "std/math" as m;               // alias binds the MODULE itself: m.floor(x)
from "std/str" import upper, split;   // bind exported symbols directly
from "std/str" import upper as up;    // symbol alias

// the same surface imports natives - no separate syntax (M2, M6):
import "raylib";                      // raylib.init_window(...)
from "raylib" import window, draw_text;

// file-relative (raw ok files, mixing allowed - M7):
import "./util.ok";
from "../lib/helpers" import tick;

// the import binds the PATH, not just the stem (M5, Q's ruling):
import "a/b";                         // the module lives at a.b - a.b.XXX
import "a/c";                         // merges: the same root binding `a`
```

- **M5, spelled precisely**: `import "a/b"` binds the FIRST path segment
  (`a`) as a namespace in the importing module's scope; the module value
  lands at `a.b`; its exports are `a.b.XXX`. a second import under the
  same root merges into the same namespace object. `as` overrides by
  binding the MODULE itself (`import "a/b" as c` -> `c.XXX` - python's
  `import a.b as c` precedent). `from "x" import y` binds the exported
  symbol directly.
- paths are **quoted strings, single or double** (M1) - same literal
  rules as strings. `import std.io` python-plain is NOT the language;
  bare identifiers after import are a hard error ("quote the path").
- imports are legal **only at module top level** in v1 (hard error
  elsewhere). the import path is a compile-time constant - there is no
  computed import, no conditional import, no importlib. at. all. this
  wave (M4, Q: "no importlib whatsoever for v1, not now pushed for later,
  it is totally fine right?" - yes: nothing in the design precludes a
  runtime lane later; the compile-time spine is the fast case by then).
- `from "x" import a, b` - symbols must be `pub` in x. `import *` is
  rejected (M8, explicitness doctrine).
- **`std` is a reserved root name** (M8 enforcement): a module that binds
  anything named `std` at top level is a hard error. the protection has
  teeth because the root-binding rule would otherwise let user code
  shadow the std namespace.
- no `__main__` magic (D9): the entry file is a plain module whose top
  level runs after its imports.
- what is an export: **`pub` at module top level** (M9). `pub fu`, `pub
  let`, `pub class`, `pub static` - the modifier already exists. everything
  else is module-private. no separate export syntax to design.
- the REPL (cli main.c) compiles per line today; the module registry
  persists per ok instance, so REPL imports resolve once and stay
  initialized across lines. no special machinery.

## 3. the semantics (compile-time, zero runtime cost - D9 carried whole)

- the compiler resolves the import graph depth-first at compile time.
  each module = one TU compiled once, keyed by RESOLVED absolute path
  (symlinks resolved; the D20 interner canonicalizes the key).
- a module's exports are a **slot array + name table** (the globals trick
  one level up). `std.io.print` after `import "std/io"` compiles to:
  module value load + slot load. no runtime lookup, ever.
- **top-level code runs once**, on first import, before the importer's
  top level. the registry caches the initialized module value. init order
  is the depth-first order. **a cycle is a hard compile error** (M13 -
  "the cycle is in YOUR design, fix the design"; D9's hard-fail doctrine,
  now Q-ruled for cycles too).
- the module value: a frozen namespace object with the export slots (the
  same shape machinery as frozen classes). an ordinary value: store it,
  pass it, put it in a map.
- binding rules: the root binding (`a`) is a scoped binding like any
  `let` - redeclaring the same root name twice in one scope, or binding
  a name that already exists in that scope, is the usual
  duplicate-binding compile error (M13, simplest way: hard error). inner
  scopes may shadow.
- natives: same graph, same forms (M2). the difference is entirely in the
  LOADER (section 7): `"raylib"` resolves to a shared object; the module
  value is built from its descriptor. `from "raylib" import draw_text`
  binds the native function directly - the static lane does not care
  that the export is a C function.
- **a native module is fully loaded at compile-time resolution** (dlopen +
  descriptor + on_load + class mounting run when the compiler resolves
  the import - the compiler needs the export table). the capability gate
  applies at the loader, so compile-time loading is gated exactly like
  runtime would be (section 6).
- stack traces and diagnostics across modules: every TU registers its
  source (the existing vm_sources table, one entry per run today, grows
  to one per module - the stale okvm.h "0.7.0 registry" comment is this
  work). file:line:col stays honest in multi-file traces.
- import depth limit: the resolution recursion shares the compiler's
  existing depth guard; a pathological graph errors instead of blowing
  the C stack.
- `glob` interplay: glob stays the process-global escape hatch (legacy +
  deliberate side-effect door). a module MAY glob-inject globals; std is
  still protected (M8). no new semantics invented here.

## 4. okproject.ok - the project shape (M10, Q's ruling)

a file at the project root, **strictly in ok** (it runs through the real
parser + VM - no second config format to maintain), mirroring pyproject.toml
in role. the CLI evaluates it in a **fresh ok instance with the FULL
stdlib and NO natives loaded** (M10: "not limited, it can do whatever
using stdlib, but it can't load native" - Q overruled my earlier
no-natives-restricted-instance lean; the no-natives half stands for v1,
the door for more is section 6's open question). it may not import (it is
a script, not a module, in v1). the CLI reads one global:

```ok
// okproject.ok
glob let project = {
  "name": "myapp",
  "version": "0.1.0",
  "ok": "0.8",             // the oklang surface this project targets

  // where the project's own ok modules live (import roots, relative to
  // this file). default ["src"] when absent.
  "modules": ["src"],

  // path deps only (M11): no registry, no downloads, no version solving.
  // a dep is a reachable directory: another okproject, a raw directory
  // of ok files, or a native module dir. mixing is fine (M7). the dep's
  // NAME is an import root: import "mathx/vec" resolves inside dep mathx.
  "deps": {
    "mathx":   {"path": "../mathx"},           // another okproject
    "helpers": {"path": "vendor/helpers"},     // raw ok files, no project
  },

  // native build targets (M12, Q's ruling: NOT coupled to cmake or gcc -
  // a declared BUILD COMMAND, whatever toolchain). the artifact is the
  // importable module.
  "natives": {
    "raylib": {
      "dir": "native/raylib",
      "build": "make -C native/raylib",         // premake/meson/cmake/shell/gcc - anything
      "artifact": "native/raylib/bin/raylib.so" // what "raylib" resolves to
    }
  }
};
```

rules:
- discovery walks UP from the ENTRY FILE (never cwd - the last implicit
  working-dir dependence dies here) to the nearest okproject.ok; that
  root governs. a raw ok file with no project above it is a standalone
  script: file-relative imports only (plus std, plus OKPATH).
- a dep that is itself an okproject recurses: its own roots/deps/natives
  apply. dep names must be unique (M13: collision = error naming both
  paths).
- **no pip, and Q is leaning "not just v1 - maybe forever"** (M11: mixed
  feelings about package managers, vendor-via-git preferred). a module is
  something reachable on disk; the day a registry is ever wanted,
  okproject grows a "source" field and nothing here precludes it. the
  doctrine is: `git clone` is the dependency manager.
- manifest evaluation errors are hard failures with the manifest's own
  path:line:col. the manifest instance's capabilities: host caps minus
  the native caps, always (section 6).

## 5. resolution (the search order - M3, M6, with Q's stress made explicit)

for an import path P from file F, **natives are first-class citizens at
every step** (M6, Q: "natives are directly importable... a native that
can be resolved per usual resolution walk can be imported"):

1. P starts with `./` or `../` (or is absolute): resolve against F's
   directory. the ladder at every candidate location: exact file, then
   P + ".ok", then P + "/index.ok", then **P + native extension
   (.so/.dll/.dylib per platform)**. raw ok files AND raw natives, no
   project needed - the mixing lane (M3: "if the native is resolvable it
   should be able to load it without an active project").
2. P starts with `std/`: the standard library (re-homed by OR-2). never
   found through steps 3-5: user code cannot shadow std (M8).
3. walk up from F to the nearest okproject.ok root R: search R's module
   roots (["src"] default) with the same ladder (incl. native
   candidates), then R's declared native targets (build if stale and
   allowed - sections 4/6/8), then R's deps by name (recursing into each
   dep's own project).
4. OKPATH (colon-separated env var) / embedder-provided extra roots.
   last, always. same ladder, native candidates included.
5. still nothing: hard error - **the 404 must teach**: the full trace of
   every location and form tried, plus the hint ("add its directory to
   OKPATH, or force-load with ok run --native"). `ok where "mathx"`
   prints the same trace without running.

- an ambiguity (two deps expose the same name) is an error naming both
  paths (M13). the fix is always explicit: `import "./vendor/x"` or
  rename one of them.
- `--force-native` (M3: "the last resort is the force"): `ok run --native
  m.so app.ok` force-loads a native module outside any project, under its
  declared name, before compile. it exists so "load ANY conformant .so
  into oklang from the CLI, no embedding shell" is always possible.
- path syntax: forward slashes only (windows included), ASCII, relative
  or root-relative. no absolute filesystem paths in import strings (an
  embedder's roots are the portability seam).

## 6. sandboxing - the capability system (Q's catch)

why mandatory, in Q's words: "since we are an embedded language, this is
mandatory. maybe scripts are being ran in constrained environments,
oklang shouldn't be the escape hatch that allows to load arbitrary native
code lol". so the capability system lands **in the same patch as the
native loader** (OR-3) - loading is born gated.

```c
// ok_specs grows a caps field; ok.h grows the control surface:
typedef enum {
  OK_CAP_NATIVE_LOAD  = 1 << 0, // dlopen natives (import + --native)
  OK_CAP_NATIVE_BUILD = 1 << 1, // run okproject build commands
  OK_CAP_HOST         = 1 << 2, // host-touching stdlib (fs/env/clock as they land)
  OK_CAP_ALL          = 0xffffffff,
} ok_cap;

uint32_t ok_caps_get(ok* p_ok);
void ok_caps_drop(ok* p_ok, uint32_t p_caps); // RATCHET: drop only, never grant
```

- **the ratchet is the security property**: caps are set at ok_create and
  can only be DROPPED afterwards. no path - not a native, not a script,
  not a manifest - can escalate. (Q asked "are there other meaningful
  ways to control this?" - the full answer:)
- **the control surfaces that matter**:
  1. the embedder's ok_specs at ok_create + ok_caps_drop (the only
     writable surface).
  2. CLI flags mapping onto the same: `--restrict` (preset: drop
     NATIVE_LOAD + NATIVE_BUILD + HOST), `--sandbox=locked` (pure
     compute - everything dropped), granular `--allow=native
     --deny=build,host`.
  3. structural: the loader vtable itself (section 7) - an embedder that
     installs a loader whose load_native is NULL has denied natives by
     construction, flags or not. belt and suspenders.
  4. rejected on purpose: environment variables (spoofable, meaningless
     once embedded, ambiguous precedence with the embedder), per-call
     caps (a script is a unit of trust, not a call), runtime grants
     (the ratchet forbids them).
- **what checks what**: import resolution reaching a native candidate ->
  NATIVE_LOAD. ok build / auto-build -> NATIVE_BUILD + the consent
  policy (below). okproject.ok evaluation -> the manifest instance gets
  host caps minus both native caps, always. host-touching stdlib as it
  lands (fs is D16 stdlib-v1) -> HOST.
- failure mode: one clear error naming the capability. `native import
  "raylib" refused: OK_CAP_NATIVE_LOAD is dropped in this ok instance.`
- **the two security questions Q raised, answered honestly**:
  - *should build processes be sandboxed?* (M12: "the build process for
    some languages allows running arbitrary code via the compiler") -
    true and unfixable at our layer: sandboxing a compiler/build system
    for real means containers/VMs, which is out of scope for a
    zero-dep language runtime. our model is consent + capability +
    documented trust boundary: `git clone` IS the trust decision; `ok
    build` is you running that repo's build script on purpose. so: the
    first build of a target asks (interactive confirm unless `--yes`;
    `--no-build` refuses and prints the teaching error), the capability
    gate stands for embedders, and we never claim isolation we don't
    have. `--restrict` exists for the paranoid/CI case.
  - *should okproject.ok itself load natives?* (M10: "think of it as a
    powerful build script but idk, maybe --restrict or something") -
    v1 stays conservative per Q's ruling: the manifest instance never
    loads natives. the door is deliberately left unlocked though: when a
    real build script genuinely needs a native, the grant is one
    capability bit on the manifest instance away, post-0.8.0, with the
    same consent story as builds. worth revisiting with a real use
    case, not before.
- different types of constraints (Q: "there should be different types of
  constrains right?"): yes - the three capability families above (load /
  build / host), because they fail differently: LOAD is the escape hatch
  (arbitrary native code in-process), BUILD is arbitrary code out-of-
  process (the compiler story), HOST is data-plane leakage (fs/env/
  network later). they compose as a bitmask; presets are sugar.

## 7. the loader - one door, three backs (M3, M14)

the VM and the compiler NEVER touch the filesystem for imports. all
loading goes through a vtable the host installs:

```c
// include/ok/ok_module.h (new, OR-3)
#define OK_NATIVE_ABI 1

typedef struct ok_native_fn_def {
  const char* name;
  uint16_t arity;             // counts the receiver where applicable; OK_NATIVE_ARITY_ANY allowed
  ok_native_fn call;
} ok_native_fn_def;

typedef struct ok_native_module {
  uint32_t abi;               // must equal OK_NATIVE_ABI, checked at load
  const char* name;           // self-declared; must match the import stem for graph loads
  const char* version;
  uint32_t fn_count;
  const ok_native_fn_def* fns;
  // optional class surface: built at load via the patch-34 door.
  bool (*mount_classes)(ok* p_ok);   // false on failure (error lane set)
  // optional lifecycle. on_load runs at load (compile-time resolution);
  // on_unload at ok_free - never mid-run (the lua lesson).
  bool (*on_load)(ok* p_ok);
  void (*on_unload)(ok* p_ok);
} ok_native_module;

// the ONE symbol the shared object must export:
// OK_API const ok_native_module* ok_native_export(const ok_api* p_api);

// the api vtable (M14: "the vm only cares about its vtable"): a dlopen'd
// module NEVER links against oklang symbols (no -rdynamic, no symbol
// collisions, several vms can coexist). it calls through this table,
// which the loader fills with the host's implementations - the same
// functions ok.h exposes for static embedders, plus the coro primitives
// (appendix A) from day one: adding vtable entries later is an ABI break,
// and future asio-style natives cannot be written without them.
typedef struct ok_api {
  uint32_t abi;               // OK_NATIVE_ABI
  // values
  ok_value (*value_string)(ok*, ok_string_view);
  ok_value (*value_number)(double);
  ok_value (*value_bool)(bool);
  ok_value (*array_new)(ok*, const ok_value*, uint32_t);
  ok_value (*map_new)(ok*, const ok_value*, uint32_t);
  bool (*map_set)(ok*, ok_value, ok_value, ok_value);
  // classes (the patch-34 door)
  ok_value (*class_new)(ok*, ok_string_view, ok_value p_parent);
  bool (*class_define_native_method)(ok*, ok_value, ok_string_view, uint16_t, ok_native_fn, bool p_pub);
  bool (*class_freeze)(ok*, ok_value);
  // errors + roots
  bool (*native_error)(ok*, ok_string_view);
  bool (*native_throw)(ok*, ok_value);
  uint32_t (*root)(ok*, ok_value);
  bool (*unroot)(ok*, uint32_t);
  // coro primitives (appendix A) - asio's future door
  ok_value (*coro_new)(ok*, ok_value p_body);
  bool (*coro_resume)(ok*, ok_value p_coro, ok_value p_arg, ok_value* p_out);
  uint32_t (*coro_status)(ok*, ok_value p_coro);
  ok_value (*coro_current)(ok*);
  // (grown carefully; every addition bumps OK_NATIVE_ABI)
} ok_api;

// the loader vtable (host installs; the CLI installs the fs default)
typedef struct ok_loader {
  bool (*read_source)(void* p_ctx, const char* p_path, ok_source* p_out);
  bool (*load_native)(void* p_ctx, const char* p_path, const ok_native_module** p_out, const ok_api** p_api);
  void* p_ctx;
} ok_loader;
void ok_set_loader(ok* p_ok, ok_loader* p_loader);
```

why a vtable and not "just read files":
- **the embedding working-dir problem, killed by construction** (Q: "when
  embedding, and want to import, we dont know what the embedders working
  dir is"). paths resolve against EXPLICIT roots (the loader's), never
  an implicit cwd. the CLI's default loader resolves the entry file's
  dir + okproject discovery + OKPATH; an embedder installs its own:
  memory-backed sources, packaged assets, wasm fetches. the language
  never guesses.
- **emscripten/wasm is a backend, not a redesign**: load_native there is
  a static registration table (no dlopen on wasm) - the vtable is the
  seam.
- testability: the e2e matrix gets a memory-backed loader test in C
  (OR-3), which is also the embedder documentation by example.

loading discipline for natives: dlopen(path, RTLD_NOW | RTLD_LOCAL) - a
module's symbols never leak into another's namespace. dlsym
("ok_native_export"), abi check first (both directions: the module may
refuse US), name check second (the path's stem and the declared name
must agree for graph imports; --native force-loads register under the
declared name). a module is loaded ONCE per ok instance (the registry).
on_load failures and abi mismatches produce one clear error: `module
'raylib': abi 0, expected 1 - rebuild against include/ok/ok_module.h`.
no dlclose mid-run ever; on_unload fires at ok_free.

## 8. the CLI (M3, M12)

```
ok run app.ok                  # imports resolve per section 5
ok run --native m.so app.ok    # force-load (M3's last resort)
ok run --restrict app.ok       # sandbox preset (section 6)
ok build [--clean|--force]     # run native build commands, refresh .okbuild/
ok where "mathx"               # print the resolution trace, exit 0/1
```

- `ok build` runs each native target's declared BUILD COMMAND (M12 -
  any toolchain) via the shell from the project root, then checks the
  declared artifact appeared. staleness: the artifact is missing, or the
  build command string changed since the last recorded success
  (.okbuild/<target>.stamp). `--clean` drops .okbuild and rebuilds,
  `--force` rebuilds unconditionally. the sandbox has no cmake and no
  make - the e2e fixtures declare raw gcc commands
  (`gcc -shared -fPIC -Iinclude ...`), which also proves the
  toolchain-agnostic claim.
- auto-build on import: only inside a project, only when OK_CAP_NATIVE_BUILD
  is up, only with consent (first build of a target confirms unless
  --yes/--quiet; --no-build turns it into the teaching error). a bare
  script NEVER builds anything (M3: scripts import prebuilt/OKPATH/
  relative natives or force-load - the standalone lane).
- the first `ok` with no arguments is the REPL (unchanged shape);
  imports work per-line through the persistent module registry.

## 9. what this wave deliberately does NOT do

- no scheduler, no async, no casio/asio (standing veto; separate
  project). the coro primitives it will need are in appendix A.
- no dynamic import / importlib - not deferred-within-the-wave, OUT of
  it (M4). a runtime lane, if ever, is a later wave's explicit design.
- no package registry, no downloads, no version solving - possibly
  forever (M11). deps are paths. vendor via git.
- no hot reload (pipeline isolation v2's question).
- no per-module GC isolation, no module unloading at runtime (on_unload
  at ok_free; a dead module stays loaded - the lua lesson).
- no conditional compilation / imports (the preprocessor is a lie).
- no star imports (M8).
- no compiled-module/bytecode cache - compiles are fast, the cache is
  complexity; revisit only if a real project hurts.
- no REPL-specific import semantics beyond the registry persistence.

## 10. the patch series - dense on purpose (Q: "the plan was too sparse")

four patches, each a full session or better, each landable in order,
each gated by the full matrix (427+ e2e x plain/threaded/asan/aggressive
+ new tests + WORKLOG entry). if a session dies mid-patch, the next one
reads this section + WORKLOG and resumes - that is the whole point.

- **OR-1 - the compile-time spine** (was P1+P2, the big one, ~600
  lines): the grammar (import/from/as, quoted paths, bare-identifier
  hard error) + tree-sitter catch-up; path normalization + the
  resolution ladder (relative + std + OKPATH, native candidates
  included); the loader vtable with the CLI's fs default (read_source
  only - load_native is OR-3's); the module registry (D20-interned
  canonical paths); TU-per-module compile against the snapshot (D8);
  the module VALUE (frozen namespace + export slots + name table); pub
  exports; **M5's path binding with root-namespace merging, `as` =
  module alias, from-import**; top-level-only enforcement; the `std`
  root reservation; cycles + collisions + shadowing as hard errors;
  the teaching 404; per-module source registration (stack traces);
  the multi-file e2e runner extension. acceptance: multi-file e2e
  (import chains, cycle errors, init order, pub/privacy, root merging,
  alias/from forms, cross-file traces), the whole old matrix green.
- **OR-2 - std re-home** (was P3, the compatibility-sensitive one):
  the std/ directory (io, math, str, array, map, coro - **coro becomes
  std.coro per R24**, its always-loaded ruling relaxed to "std module,
  preloaded by default for now"); natives re-registered as module
  members; **flat globals stay as deprecated aliases - removal is the
  NEXT stdlib version's call, not 0.7.0's** (Q's sub-ruling: "flat ones
  keep it for now, we work on this in stdlib next version"). the
  print-statement deprecation plan (and the %g default's replacement by
  std.io's formatting) is written down here as that version's scope.
  acceptance: old scripts unchanged (aliases), new-style scripts pass,
  the std module table documented.
- **OR-3 - sandbox + natives + the embedder loader** (was P4+P7 + the
  new sandbox section; loading is born gated): the capability system
  FIRST (ok_specs caps, the ratchet, ok_caps_get/drop, CLI --restrict/
  --sandbox/--allow/--deny, gating the host-touching surface); then
  ok_module.h (the descriptor + the ok_api vtable incl. the coro
  primitives of appendix A); the loader's load_native (RTLD_NOW |
  RTLD_LOCAL, abi + name checks, the error messages); registration
  under the module namespace; class mounting through the patch-34 door;
  lifecycle hooks; `ok run --native`; a hello-native sample module (one
  function + one class) + e2e with a declared gcc build; the
  memory-backed loader C test (the embedder example). acceptance:
  `from "hello" import greet; greet("x")` end to end; abi-mismatch and
  bad-symbol errors are the documented messages; --restrict refuses
  with the capability error; e2e runs an import graph fully from
  memory with zero fs calls.
- **OR-4 - okproject.ok + the build pipeline** (was P5+P6): manifest
  discovery (walk up from the ENTRY file), the evaluator (full-stdlib
  no-natives instance, reads the `project` global), module roots, path
  deps (project/raw/native, recursive), native targets with declared
  build commands, the consent policy, .okbuild staleness stamps, ok
  build/where, auto-build-on-import, the fixture farm (project with
  deps-on-project, raw-dir dep, missing dep, ambiguous dep, nested
  projects, restricted-mode refusals). acceptance: the farm is green
  and every failure mode prints its teaching error.

order rationale: the spine first (everything else is a producer or
policy over the module VALUE it defines); std second so the surface is
dogfooded by our own stdlib before natives land; sandbox+natives third
so loading is gated the moment it exists (Q's "right when we allow for
loading (that is now!)"); the project shape last because it is pure
policy over mechanics that all exist by then.

## 11. the rulings, recorded (M1-M14 CLOSED + sub-rulings, Q 2026-11-03)

- **M1** quoted paths ('' or ""), python-plain is not the language. ✓
- **M2** one import surface for ok files and natives; the resolver
  decides, the syntax never distinguishes. ✓
- **M3** the CLI loads natives directly: per project rules, or - with no
  project - via the usual OKPATH/relative walk; unresolvable errors;
  `--force-native` is the last resort. a resolvable native loads with
  no active project. ✓
- **M4** imports are compile-time only for v1. no importlib whatsoever,
  pushed for later. ("totally fine right?" - yes, answered in section 2.)
- **M5** `import "a/b"` binds the path: the module becomes a.b.XXX
  (root-namespace binding, merging). `as` overrides. ✓
- **M6** resolution order as designed, **natives directly importable
  through the same walk** - Q stressed this explicitly; the ladder
  includes native candidates at every step. ✓
- **M7** raw ok files, project ok files, and natives (raw or project)
  are all accepted as deps. ✓
- **M8** std is protected (root name reserved, unshadowable); NO
  wildcard imports. ✓
- **M9** `pub` is the export rule; no new syntax. ✓
- **M10** okproject.ok is a single ok file evaluated in a sandboxed ok
  instance - not limited (full stdlib), but it cannot load native. the
  natives-in-manifest question stays open with a one-bit door (section
  7). ✓
- **M11** no package manager - maybe not just v1, maybe forever;
  vendor via git. ✓
- **M12** native targets declare a BUILD COMMAND - whatever build
  system (premake, mason, cmake, raw shell, raw compiler). build
  security: consent + capability, no pretended isolation (section 6). ✓
- **M13** collisions AND circular imports: the simplest way - hard
  error. ✓
- **M14** the VM is concerned with none of this; it only cares about
  its vtable. (the loader vtable + the ok_api vtable are the whole
  boundary.) ✓
- sub-rulings: coro's home is **std.coro** (OR-2); the flat stdlib
  globals **stay for now** - the flattening work is the NEXT stdlib
  version's. ✓

## appendix A - the coro C API ("vm_tick"), specced for patch 42

Q's ask: "there is some missing api that comes with coros, which is
vm_tick or something - figure it out and report." the investigation,
recorded here because ok_module.h's vtable depends on it:

**what exists**: the whole engine is there - coro_drive (the resume
driver: state swap, parked-resumer chain, R22 death sweep, error
transplant), the yield barrier discipline, the status machine - but the
ONLY entry points are the script-facing natives: `coro(fu)`,
`c.resume([v])`, `c.status()`. **ok.h has zero coro surface.** an
embedder today cannot create, resume, or even inspect a coroutine from
C - dorg3 would have to keep a trampoline closure in script just to
drive one per frame.

**what "vm_tick" actually is here**: NOT a VM ready-queue pump - a
scheduler is stdlib (casio, later) by standing veto, and the pump is
its loop calling resume. what is missing is the primitive set that pump
is built from, plus one real piece of VM machinery:

```c
// ok.h additions (patch 42 proposal):
ok_value  ok_coro_new(ok* p_ok, ok_value p_body);           // coro(fu) from C
bool      ok_coro_resume(ok* p_ok, ok_value p_coro, ok_value p_arg, bool p_has_arg, ok_value* p_out);
                                                            // the game-loop primitive:
                                                            // while (running) { ok_coro_resume(...); render(); }
uint32_t  ok_coro_status(ok* p_ok, ok_value p_coro);        // 0=ready 1=suspended 2=running 3=dead
ok_value  ok_coro_current(ok* p_ok);                        // inside a native: the running coro (null on main)
bool      ok_native_suspend(ok* p_ok);                      // the deep one - see below
```

- ok_coro_resume is a thin, honest wrapper over coro_drive with the C
  boundary discipline of ok_call (errors land in the ok_call error
  lane, read them with ok_last_error; a dead coro answers null per
  R21; a running one refuses). status/current are trivial.
- **ok_native_suspend is the one real piece**: a native (future asio's
  `sleep`, `await`) needs to park its CALLER. a C function cannot yield
  across its own frame (the yield barrier doctrine, unchanged) - but it
  can request suspension AT THE NATIVE RETURN BOUNDARY: set the
  pending-suspend flag; when the native returns, the dispatch loop's
  OP_CALL completion check parks the coro exactly like OP_YIELD would
  (resume target = the call's result register; the wake value arrives
  with the next resume). the barrier still guards it - a suspend
  request from under a C-driven region (operator dispatch, defer
  unwind, construction) is refused with the same error a yield there
  gets. this is ~40 lines in the OP_CALL completion path + the flag.
- **why patch 42 and not later**: OR-3's ok_api vtable must carry the
  coro primitives from day one - adding vtable entries later is an ABI
  break, and natives that park/resume coros (the entire asio future)
  cannot be written without them. landing the C API first also gives
  the 0.7.0 lane its embedder story: the game loop drives coros
  directly, no trampolines.
- tests: the e2e matrix is script-only today; the coro C API needs a
  tiny C harness (tests/capi/, one main.c driving ok_coro_* through a
  real script with yields + error death + suspend-request lanes) run
  by the build flavors. that harness then doubles as OR-3's
  memory-backed-loader test bed. (proposal - Q rules on the shape.)

## 12. the future doors this opens (recorded, not scheduled)

- **raylib as an oklang module**: ok_class_new mounts real classes with
  native methods; raylib's C API is mostly functions + structs - a
  mechanical binding generator over ok_module.h is a weekend, and then
  oklang writes games with no shell. the first real dogfood of all of
  the above.
- **emscripten loop APIs via import**: the wasm loader backend +
  `import asio` (casio, later) gives the browser main-loop model
  natively - the web fps wobble fix rides the same seam.
- **the static-site / portfolio pledge**: fs_ + modules = the
  static-site generator in pure oklang.
- **the dynamic lane, if ever wanted**: import(name) returning a module
  value with name-based access + inline caching - D9's escape hatch,
  now explicitly post-0.8.0.
- delta blue stays the 0.9.0 meter: modules land, then the
  allocation-shaped gap closes, then the language is in the race.
