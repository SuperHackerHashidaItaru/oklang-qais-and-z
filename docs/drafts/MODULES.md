# MODULES — the 0.8.0 wave, codename "O Resolution" (final)

status: FINAL, amended twice. the M1-M14 rulings came back (2026-11-03,
recorded verbatim in section 11), the sandboxing gap Q caught is designed
in (section 6), the patch series is re-densed per Q's "the plan was too
sparse" (4 patches, was 8 - section 10). the pre-modules work Q ordered
landed first: string interning is patch 41 (D20, 427/427 x5) and the coro
C API is patch 42 (D21, LANDED - appendix A), so ok_module.h's vtable
inherits it instead of breaking later. **OR-1 is LANDED** (session 36,
patches 43 + 44, Q applied to modules-staging: the spine, 471/471 x5).
**OR-3 is LANDED** (session 38, patches 47 + 48: the allocator discipline
Q caught in review, the registry read api, then sandbox + natives + the
embedder loader, 488/488 x7).

amendments, round two (2026-11-03, Q's landing review):
- RULED: **project.ok exports its table with module-level `pub`, not
  `glob`** - "it teaches bad practice of using glob to export stuff".
  the manifest is a plain module and the CLI harvests its pub export
  (section 4, folded in). the sandbox would have kept the global from
  leaking anyway, but the manifest should model the right shape.
- RULED: **the diagnostic doctrine** - error messages carry the helpful
  half only, no editorial asides, and standard punctuation only: comma,
  colon, period. no dash, no em dash, no semicolon (D22, patch 45 swept
  the whole tree; new code follows it from here on).

amendments, round three (2026-11-03, Q's OR-3 green light):
- RULED: **everything we have, exposed to the abi** - "we should support
  everything we have and expose to the abi". the four OPEN items are IN
  (the read side, type/class identity, calls, the single-integer
  versioning), and the first real module forced two additions the draft
  missed: api->view (string-view construction, a native cannot even
  spell its arguments without it) and api->module_export (the mount
  callback's export primitive, classes and constants through it). the
  table is static and stateless: stashed at export time, valid for the
  process lifetime (D23).
- RULED: **the user's allocators are honored everywhere** - Q's review
  caught plain mallocs in okmodules.c ("we have own allocators that we
  get from user and they should be honored"). patch 47 routed the whole
  module spine through them, loader contract included.
- RULED: **the module registry read api** - "there should be an api for
  modules export/imports otherwise how is cli gonna even read the
  exported table?" ok.h grows it (patch 47): the loaded list, each
  module's import edges, any module value's export enumeration.
- NOTE (implementation deltas from the draft's section 7, all in patch
  48): the ok_api table carries value_null, value_as_native_value and
  native_value_new beyond the draft's list (the everything ruling); the
  ladder probes ok-module candidates through the loader's read instead
  of the library stat()-ing files (M14 taken literally - the memory
  loader in the capi test is the proof); realpath stays a
  canonicalization AID with a pure fallback (virtual paths key by the
  resolved path itself); a loadable-but-broken native is a hard error
  with the loader's own reason as the note; the descriptor's mount
  callback exports through api->module_export rather than mutating the
  namespace (the namespace freezes when mount returns).

amendments (2026-11-03, Q's review round on this draft - the two RULED
ones are folded in below, the api-surface items are answered and marked
OPEN for Q at section 11's end):
- RULED: the manifest is **project.ok** (was okproject.ok) - "no need for
  the ok prefix, as the file ext gives it".
- RULED: **coro stays a core native** like str/num/bool/map/array/type -
  "since it is inside the api, it shouldn't be gated in std". the std.coro
  re-home (R24's lean) is OFF; OR-2 shrinks to the std/ re-home of the
  flat stdlib.
- RULED: platform independence by construction - the **OK_API** export
  macro (msvc dllexport/dllimport) and **dlopen/dlsym hidden behind a
  compile-time wrapper** (windows LoadLibrary, everything posix-ish
  dlopen - section 7).
- Q-proposed, Z-endorsed (design in section 7): the host provides
  **dlopen/dlsym like malloc** with the platform wrapper as fallback, and
  can turn dynamic loading off entirely - unsupported platforms run the
  language, and the off-switch doubles as the structural sandbox denial.
- OPEN for Q: the ok_api vtable's read side + type/class identity entries
  (section 7) and the versioning stance (single bumped integer, no semver).

renumbering note: this wave was drafted as 0.6.0; Q's tidy lane (classes
fix + defer continuation + the rename + interning + the coro C API) is
v0.7.0 now - **named "Epigone"** (Q's pick, recorded) - so modules is
**0.8.0**. one stale in-tree reference (okvm.h's "the 0.7.0 registry"
comment) is OR-1 scope. the codename is Q's pick: "O Resolution"
(wilderun, veil of imagination - we are, after all, resolving paths).

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
- the 0.7.0 lane (Q's tidy branch, "Epigone"): defer continuation (D17,
  patch 39), the type/object rename (D18, patch 40), universal string
  interning (D20, patch 41), and the coro C API (D21, patch 42 - LANDED
  on modules-staging, appendix A). this wave stacks on top of all of it.
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

## 4. project.ok - the project shape (M10, Q's ruling; renamed per Q's
review round: "no need for the ok prefix, as the file ext gives it")

a file at the project root, **strictly in ok** (it runs through the real
parser + VM - no second config format to maintain), mirroring pyproject.toml
in role. the CLI evaluates it in a **fresh ok instance with the FULL
stdlib and NO natives loaded** (M10: "not limited, it can do whatever
using stdlib, but it can't load native" - Q overruled my earlier
no-natives-restricted-instance lean; the no-natives half stands for v1,
the door for more is section 6's open question). it may not import (it is
a script, not a module, in v1). the table is a module-level pub export
(amended round two: Q ruled out the original `glob let` shape - "it
teaches bad practice of using glob to export stuff". the manifest runs
sandboxed so the global would never leak anyway, but the manifest should
model the right shape: a plain module exporting one pub table, harvested
by the same machinery OR-1 built for every other module):

```ok
// project.ok
pub let project = {
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
  working-dir dependence dies here) to the nearest project.ok; that
  root governs. a raw ok file with no project above it is a standalone
  script: file-relative imports only (plus std, plus OKPATH).
- a dep that is itself a project recurses: its own roots/deps/natives
  apply. dep names must be unique (M13: collision = error naming both
  paths).
- **no pip, and Q is leaning "not just v1 - maybe forever"** (M11: mixed
  feelings about package managers, vendor-via-git preferred). a module is
  something reachable on disk; the day a registry is ever wanted,
  project.ok grows a "source" field and nothing here precludes it. the
  doctrine is: `git clone` is the dependency manager.
- manifest evaluation errors are hard failures with the manifest's own
  path:line:col, and so is a manifest that exports no pub `project`
  table (the evaluator harvests the export through the ordinary OR-1
  module machinery). the manifest instance's capabilities: host caps
  minus the native caps, always (section 6).

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
3. walk up from F to the nearest project.ok root R: search R's module
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

## 7. the loader - one door, three backs (M3, M14; platform independence
per Q's review round)

the VM and the compiler NEVER touch the filesystem for imports. all
loading goes through a vtable the host installs:

```c
// include/ok/ok_module.h (new, OR-3)
#define OK_NATIVE_ABI 1

// platform-independent export/import: the one symbol a native shared
// object exposes. msvc needs dllexport/dllimport; everywhere else the
// default visibility is fine (and -fvisibility=hidden builds export only
// this). the wrapper is compile-time - no runtime dispatch.
#if defined(_WIN32)
#  if defined(OK_MODULE_BUILDING)
#    define OK_API __declspec(dllexport)
#  else
#    define OK_API __declspec(dllimport)
#  endif
#else
#  define OK_API __attribute__((visibility("default")))
#endif

typedef struct ok_native_fn_def {
  const char* name;
  uint16_t arity;             // counts the receiver where applicable; OK_NATIVE_ARITY_ANY allowed
  ok_native_fn call;
} ok_native_fn_def;

typedef struct ok_native_module {
  uint32_t abi;               // must equal OK_NATIVE_ABI, checked at load
  const char* name;           // self-declared; must match the import stem for graph loads
  const char* version;        // the MODULE's own version string (display only)
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

// the type tag every value answers - the read side's discriminator.
typedef enum {
  OK_TYPE_NIL, OK_TYPE_BOOL, OK_TYPE_NUM, OK_TYPE_STR,
  OK_TYPE_ARRAY, OK_TYPE_MAP, OK_TYPE_FN, OK_TYPE_CLASS,
  OK_TYPE_INSTANCE, OK_TYPE_CORO, OK_TYPE_NATIVE,
} ok_type_tag;

// the api vtable (M14: "the vm only cares about its vtable"): a dlopen'd
// module NEVER links against oklang symbols (no -rdynamic, no symbol
// collisions, several vms can coexist). it calls through this table,
// which the loader fills with the host's implementations - the same
// functions ok.h exposes for static embedders, plus the coro primitives
// (appendix A, patch 42, LANDED) from day one: adding vtable entries
// later is an ABI break, and future asio-style natives cannot be written
// without them.
//
// [amended per Q's review round - the read side and the type/class
// identity entries are NEW here, marked below; OPEN for Q at section 11]
typedef struct ok_api {
  uint32_t abi;               // OK_NATIVE_ABI
  // values: the write side (construct)
  ok_value (*value_string)(ok*, ok_string_view);
  ok_value (*value_number)(double);
  ok_value (*value_bool)(bool);
  ok_value (*array_new)(ok*, const ok_value*, uint32_t);
  ok_value (*map_new)(ok*, const ok_value*, uint32_t);
  bool (*map_set)(ok*, ok_value, ok_value, ok_value);
  // values: the read side (inspect + unwrap) - NEW, the draft's gap:
  // a native that cannot read its arguments can only build values for
  // nobody. mirrors ok.h's as/is families one to one.
  ok_type_tag (*value_type)(ok_value);            // the one-call discriminator
  double (*value_as_number)(ok_value);
  bool (*value_as_bool)(ok_value);
  ok_cstring_view (*value_as_string)(ok_value);   // owned by the value, never freed by the native
  uint32_t (*array_len)(ok_value);
  ok_value (*array_get)(ok_value, uint32_t);      // out of bounds: null
  uint32_t (*map_len)(ok_value);
  bool (*map_get)(ok_value, ok_value, ok_value*); // false: absent/illegal/not-a-map
  // type + class identity - NEW, Q's question answered yes (section 11):
  // a native type-checks its arguments constantly, and a native class
  // must recognize instances of itself (and of user subclasses).
  // identity itself stays word equality (===): free, no entry needed.
  ok_value (*classof)(ok*, ok_value);             // the class value; `type` for classes themselves
  bool (*is_instance)(ok*, ok_value, ok_value p_class);
  ok_cstring_view (*class_name)(ok*, ok_value p_class);
  // calls - NEW: a native invoking a script callback (sort comparators,
  // event hooks; the same shape ok_call gives static embedders)
  bool (*call)(ok*, ok_value p_callee, uint8_t p_argc, const ok_value* p_argv, ok_value* p_out);
  // classes (the patch-34 door)
  ok_value (*class_new)(ok*, ok_string_view, ok_value p_parent);
  bool (*class_define_native_method)(ok*, ok_value, ok_string_view, uint16_t, ok_native_fn, bool p_pub);
  bool (*class_freeze)(ok*, ok_value);
  // errors + roots
  bool (*native_error)(ok*, ok_string_view);
  bool (*native_throw)(ok*, ok_value);
  uint32_t (*root)(ok*, ok_value);
  bool (*unroot)(ok*, uint32_t);
  // coro primitives (appendix A, patch 42, LANDED in ok.h) - asio's door
  ok_value (*coro_new)(ok*, ok_value p_body);
  bool (*coro_resume)(ok*, ok_value p_coro, ok_value p_arg, bool p_has_arg, ok_value* p_out);
  uint32_t (*coro_status)(ok*, ok_value p_coro);
  ok_value (*coro_current)(ok*);
  // (grown carefully; every addition bumps OK_NATIVE_ABI - see the
  // versioning note below)
} ok_api;

// the dynamic-library hooks - Q's proposal, endorsed: the host provides
// dlopen/dlsym like it provides malloc, the platform wrapper is the
// DEFAULT, and NULL (or the OK_CAP_NATIVE_LOAD drop) turns dynamic
// loading off entirely. this is how the language runs on platforms with
// no loader at all (consoles, some embedded, wasm-without-dlopen): the
// host either supplies the hooks from its own packaging or accepts
// static natives only - and it is the structural sandbox denial from
// section 6.3, made concrete: an embedder that wants an audited,
// whitelisted, or memory-backed loader installs one here.
typedef struct ok_dylib {
  // each returns NULL/void* with a message in p_err on failure
  void* (*open)(void* p_ctx, const char* p_path, char* p_err, size_t p_err_capacity);
  void* (*sym)(void* p_ctx, void* p_handle, const char* p_name, char* p_err, size_t p_err_capacity);
  void (*close)(void* p_ctx, void* p_handle);
  void* p_ctx;
} ok_dylib;
// ok_specs grows `const ok_dylib* dylib;` - NULL means the platform
// default (the compile-time wrapper). the host turns loading OFF by
// dropping OK_CAP_NATIVE_LOAD (the capability gate checks first) or by
// installing a dylib table whose open always fails with its own message.

// the loader vtable (host installs; the CLI installs the fs default)
typedef struct ok_loader {
  bool (*read_source)(void* p_ctx, const char* p_path, ok_source* p_out);
  bool (*load_native)(void* p_ctx, const char* p_path, const ok_native_module** p_out, const ok_api** p_api);
  void* p_ctx;
} ok_loader;
void ok_set_loader(ok* p_ok, ok_loader* p_loader);
```

platform wrapper (compile-time, one translation unit, no runtime
dispatch - the answer to "what others are, i am only familiar with
linux's"): linux, the bsds, solaris/illumos, aix, haiku, and macos all
speak the **dlopen family** (dlfcn.h - mach-o on mac uses the same api),
windows is **LoadLibrary/GetProcAddress/FreeLibrary** with GetLastError
for the error string, and everything else (consoles, bare newlib, wasi
without a dynamic linker) has NOTHING - which is exactly the case the
host-provided ok_dylib hooks exist for. two real implementations, one
ifdef.

**versioning** (Q: "is it just a number that we bump? no sem ver and
compatibility stuff??") - proposal, OPEN for Q: yes, exactly that.
`OK_NATIVE_ABI` is a single integer in ok_module.h; any change to the
vtable, the descriptor layouts, or the tag enum bumps it; every module
carries the number it was built against and the loader checks BOTH
directions at load; mismatch refuses with the rebuild message. no semver,
no compat matrix, no deprecation cycle - pre-1.0 nobody exists to be
compatible with, and the honest stance is "rebuild your natives". the
one cheap discipline we keep anyway: vtable entries are APPEND-ONLY
(never repurposed, never reordered), so a generous "host newer than
module" policy is a later option, not a redesign.

why a vtable and not "just read files":
- **the embedding working-dir problem, killed by construction** (Q: "when
  embedding, and want to import, we dont know what the embedders working
  dir is"). paths resolve against EXPLICIT roots (the loader's), never
  an implicit cwd. the CLI's default loader resolves the entry file's
  dir + project.ok discovery + OKPATH; an embedder installs its own:
  memory-backed sources, packaged assets, wasm fetches. the language
  never guesses.
- **emscripten/wasm is a backend, not a redesign**: load_native there is
  a static registration table (no dlopen on wasm) - the vtable is the
  seam.
- testability: the e2e matrix gets a memory-backed loader test in C
  (OR-3), which is also the embedder documentation by example.

loading discipline for natives: open with the "local, resolve-now"
mode (RTLD_NOW | RTLD_LOCAL on the dlopen family, LoadLibrary on
windows - via the ok_dylib hooks, never called directly), a module's
symbols never leak into another's namespace. resolve
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

- **OR-1 - the compile-time spine** — **LANDED (session 36, patches 43 +
  44, on modules-staging: 44 module tests, 427 -> 471, gates 471/471 x5,
  the whole spine as scoped below, M5's root binding with single-segment
  paths binding the module itself)** — (was P1+P2, the big one, ~600
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
- **OR-2 - std re-home** — **LANDED (session 37, patch 46: the built-in
  std registry, never the filesystem, the shadow test proves OKPATH
  cannot touch it; 10 std tests, 481/481 x5)** — (was P3, the
  compatibility-sensitive one;
  SHRUNK by Q's review-round ruling - coro stays a core native, it does
  NOT move): the std/ modules with the natives re-registered as module
  members; **coro is NOT part of this** - it is native like
  str/num/bool/map/array/type, it lives in the api, and it is not gated
  in std (R24's std.coro home is overruled by the ruling that the api
  carries it). **flat globals stay as deprecated aliases - removal is
  the NEXT stdlib version's call, not this wave's** (Q's sub-ruling:
  "flat ones keep it for now, we work on this in stdlib next
  version"). implementation notes: the std modules are built-in
  (registry entries keyed "std/<stem>", built lazily on first import,
  the members harvested from the natives' hidden globals into the same
  sealed-namespace shape file modules get); one grammar delta rode
  along, `print` is a keyword and std.io exports it so member position
  and from-import names accept the print keyword (the `operator`
  precedent).

  **the std module table** (documented per the acceptance bar):

  | module | members | notes |
  |---|---|---|
  | std.array | len, push, pop, remove | the array natives, receiver-first |
  | std.map | len, remove, get | the map natives |
  | std.io | print | the statement's function face, same hook, same %g |
  | std.math | (empty) | opens with the stdlib growth version |
  | std.str | (empty) | opens with the stdlib growth version |

  **the next stdlib version's scope, written down here as ruled**: the
  print STATEMENT deprecates in favor of std.io.print (one release of
  warning docs first), and the %g number default replaces with
  std.io's own formatting (the shortest-exact form). the flat aliases
  (the seed names, the method spellings) ride the same deprecation
  window. none of this is 0.8.0 behavior: the aliases all work, the
  removals are the next version's to schedule.
- **OR-3 - sandbox + natives + the embedder loader** — **LANDED (session
  38, patch 48, on modules-staging: 488/488 x7, capi 201 x5, the sample
  module + the memory-loader test as the embedder example, the whole
  scope below incl. the everything-in-the-abi ruling and the two
  additions it forced, view + module_export)** — (was P4+P7 + the
  new sandbox section; loading is born gated): the capability system
  FIRST (ok_specs caps, the ratchet, ok_caps_get/drop, CLI --restrict/
  --sandbox/--allow/--deny, gating the host-touching surface); then
  ok_module.h (the OK_API export macro, the descriptor, the ok_api
  vtable incl. the coro primitives of appendix A and - pending Q's nod
  on the OPEN items - the read side + type/class identity + call, the
  ok_dylib host hooks, the single-integer ABI); the loader's load_native
  (via the dylib hooks, abi + name checks, the error messages); registration
  under the module namespace; class mounting through the patch-34 door;
  lifecycle hooks; `ok run --native`; a hello-native sample module (one
  function + one class) + e2e with a declared gcc build; the
  memory-backed loader C test (the embedder example). acceptance:
  `from "hello" import greet; greet("x")` end to end; abi-mismatch and
  bad-symbol errors are the documented messages; --restrict refuses
  with the capability error; e2e runs an import graph fully from
  memory with zero fs calls.
- **OR-4 - project.ok + the build pipeline** (was P5+P6): manifest
  discovery (walk up from the ENTRY file), the evaluator (full-stdlib
  no-natives instance, harvests the manifest's pub `project` export),
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
- **M10** project.ok (renamed from okproject.ok per Q's review round:
  "no need for the ok prefix, as the file ext gives it") is a single ok
  file evaluated in a sandboxed ok instance - not limited (full stdlib),
  but it cannot load native. the natives-in-manifest question stays open
  with a one-bit door (section 7). ✓
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
  [AMENDED 2026-11-03, Q's review round on this draft: the coro half is
  OVERRULED - **coro stays a core native**, like str/num/bool/map/array/
  type: "since it is inside the api, it shouldn't be gated in std". it
  remains always-loaded, the eighth builtin class, and the ok_api vtable
  carries its primitives (appendix A, patch 42, LANDED). the flat-alias
  half stands unchanged.]

## 11a. this round's new rulings + the OPEN items (2026-11-03, Q's review)

RULED (folded into the draft above):
- **project.ok** - the manifest filename; the .ok extension already
  says it.
- **coro is core, not std** - see the sub-ruling amendment above.
- **platform independence by construction** - OK_API for dynamic
  export/import (msvc), dlopen/dlsym hidden behind the compile-time
  wrapper (windows LoadLibrary, dlopen family everywhere else).
- **host-provided dynamic loading** (Q's proposal, Z-endorsed): the
  ok_dylib hooks in ok_specs, malloc-style - host-provided with the
  platform wrapper as default fallback, plus the ability to turn the
  feature off entirely. unsupported platforms run the language; the
  off-switch is the structural sandbox denial.

RULED (landing review, round two, 2026-11-03):
- **the manifest exports with module-level `pub`, not `glob`** - glob is
  the process-global escape hatch and the manifest should not teach it.
  section 4 is folded; OR-4's evaluator harvests the pub export through
  the ordinary module machinery.
- **the diagnostic doctrine** (D22): error messages keep the helpful
  half and drop the editorial ("python's plain import is not the
  language" is out; "spell it import \"a/b\"" stays), and the
  punctuation is comma, colon, period only - no dash, no em dash, no
  semicolon. patch 45 swept the existing tree; it binds all future
  diagnostics.

OPEN for Q (Z's recommendations, awaiting the nod before OR-3 finalizes
the vtable):
[ANSWERED 2026-11-03, Q's green light: "we should support everything we
have and expose to the abi" - all four IN, landed in patch 48. the two
entries the plan missed (view, module_export) rode along, D23.]:
- **the vtable read side** (value_type / as_number / as_bool / as_string
  / array_len / array_get / map_len / map_get): the draft shipped
  write-only, which is not enough - a native that cannot read its
  arguments can only build values for nobody. recommendation: IN.
- **type + class identity** (classof / is_instance / class_name): Q
  asked "should they be in the api?" - recommendation: YES. every
  nontrivial native type-checks its arguments, and a native class must
  recognize its own instances (and user subclasses) - the raylib door
  does not work without it. identity itself stays word equality (===),
  no entry needed. what stays OUT of v1: method enumeration, dynamic
  member get/set, subclass creation from native - each is a later ABI
  bump if a real need shows up.
- **calls** (call, ok_call's shape): natives invoking script callbacks -
  sort comparators, event hooks, the future asio. recommendation: IN.
- **versioning**: a single OK_NATIVE_ABI integer, bumped on any layout
  change, checked both directions at load, no semver and no compat
  promises pre-1.0 ("rebuild your natives" is the honest stance); the
  append-only discipline on vtable entries keeps a softer policy a
  later option, not a redesign.

## appendix A - the coro C API ("vm_tick") - LANDED as patch 42 (D21)

Q's ask: "there is some missing api that comes with coros, which is
vm_tick or something - figure it out and report." the investigation,
recorded here because ok_module.h's vtable depends on it. status: LANDED
on modules-staging (c93995d), ok.h + okvm.c + tests/capi/. the shape
survived contact with one test-authored correction and one discovery
about the draft's own test plan (both in the deltas below).

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
- **implementation deltas from this spec** (LANDED):
  - the park sites are the six native call-completion arms - OP_CALL,
    OP_CALL_LONG, OP_CALL_SPREAD, OP_CALL_SPREAD_LONG, OP_INVOKE,
    OP_SUPER_INVOKE - one flag check each on the hot path, after the
    existing post-call refresh. delta blue measured flat (baseline
    617.8ms vs 614.4ms median at -O0, 251.5ms at -O2 - within noise;
    the session-33 "249.7ms" was an -O2 build all along).
  - request-time validation (owner coro, native depth > 1, yield
    barrier) guarantees the flag never outlives the native flight that
    set it - no completion arm outside the six can ever see it, so
    there is no stale-flag lane to clean.
  - the barrier refusals spell "suspend" ("cannot suspend from inside
    an operator method.") - same kind (VALUE), same lanes as the yield
    errors, but the operation is named in the message.
  - the harness is tests/capi/src/main.c, PURE C99 (the public header
    stays C-clean - that is now pinned by a build, not a claim), 140
    checks: construct/drive/two-way channel, R21 + R22 from the C lane
    (the death sweep's defers observable through the print hook), the
    park at every call shape (plain, spread, method via a C-defined
    class, normal-exit defer), nested resume from inside a native with
    the suspended inner coro ESCAPING its dead creator to the host,
    self-resume refusal, and the three barrier refusals. it doubles as
    OR-3's memory-backed-loader test bed when the loader lands.
  - caught en route: the embed test still said `Object` - patch 40's
    rename missed it, and the last two sessions' gates quoted e2e only
    so it rotted silently. fixed; the unit runner lane (which builds
    the embed test) is part of the receipts again.
  - gates: e2e 427/427 x5 (plain, threaded, asan, aggressive-gc,
    asan+aggressive) + unit x3 / embed / capi x5 (gcc, asan, asan-aggr,
    threaded-asan, threaded-asan-aggr) + delta blue correct at 14065400.

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
