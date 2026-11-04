# oklang: master 0.4.0 → modules-staging 0.8.0 — the four lanes, a reading guide

status: the complete walkthrough of everything that stacked on master since
the errors lane, written to be read whole. Q last reviewed at master v0.4.0
(Eyrie, patches 23-28, e2e 342x4). since then four lanes landed on
modules-staging, applied patch-by-patch but never reviewed as a whole:
classes (0.5.0, Citadel), coros (0.6.0), Epigone (0.7.0, coro's upper
stretch), and O Resolution (0.8.0, modules). this guide is that whole review, ending
with today's two follow-ups: pub members (patch 51) and the semver ABI
(patch 52). the design records are CLASSES-0.5.0-draft.md, COROS-draft.md,
MODULES.md (the M/D/R rulings); this is the code-adjacent story. every
claim was verified against the tree at `973bcd0`; the gates at the end are
the receipt.

read order for a review pass: §0, then §4 (modules - the biggest, newest),
then §1 (classes - the semantic core everything else leans on), then §9
(what to review hardest) before diving anywhere else.

---

## §0 — the shape of the climb

| lane | version | patches | one line | e2e at landing |
|---|---|---|---|---|
| classes | 0.5.0 "Citadel" | 30-37 | frozen classes, operators, the member ring, natives as classes | ~400 |
| coros | 0.6.0 | 38 | stackful coroutines, per-coro frames, the yield barrier | ~410 |
| Epigone | 0.7.0 | 39-42 | coro's upper stretch: defer continuation, the rename, universal interning, the coro C API | 427 |
| O Resolution | 0.8.0 | 43-50 | the module system: compile-time spine, std re-home, sandbox + natives, project.ok | 501 |
| follow-ups | - | 51-52 | pub members, the semver ABI | 510 |

the through-line, so the review has a compass: 0.4.0 made errors honest,
0.5.0 made values shaped, 0.6.0 made control suspendable, 0.7.0 paid the
debts the first three opened, 0.8.0 made the language standalone - write
ok files, import ok files, import native C modules, build what needs
building, all from the oklang CLI with a sandbox around it. the two
follow-ups close the doors Q's landing reviews opened: members went pub
without getters, and the ABI learned to grow without breaking.

numbers, for the impatient: e2e **510x7** at `973bcd0` (plain, threaded,
asan, aggressive, asan-aggr, threaded-asan, threaded-asan-aggr; 342x4 at
master 0.4.0), capi **324x5** (the pure-C99 public-header harness), unit
+ embed green, delta blue correct at 1.40654e7. both new patches verified
apply-clean in sequence on patch-50's head in a scratch worktree.

what did NOT change: the embedding contract's shape (ok_create / ok_run /
ok_call / ok_free, print hooks, error lanes - 0.4.0's §5 still reads
true), the snapshot-swap compile pipeline (D8), the flat dispatch loop,
the no-dependency stance (C99, one library, dlopen behind a wrapper),
and the culture: every patch lands with tests in the same commit, every
landing runs the full matrix, every ruling is recorded verbatim.

---

## §1 — 0.5.0 "Citadel": classes (patches 30-37)

**the problem it kills**: dorg3's #1 pain - parallel arrays of ids with
every shape hand-rolled. also the roadmap's promise (D10): the 1.0
centerpiece, since operators, conversions, and the module namespace
machinery all turned out to hang off it.

**the model, in one screen**:

```ok
class Point {
  pub let mut x;          // patch 51: fields can be pub (see §5)
  pub let mut y;
  let label = "p";        // private: the ring keeps it

  fu init(x, y) { this.x = x; this.y = y; }

  pub fu sum() { return x + y; }
  pub fu operator +(other) { return Point(x + other.x, y + other.y); }
  pub fu operator str() { return label + " " + (x as str); }
  pub static fu origin() { return Point(0, 0); }
  pub static let mut made = 0;
}

class Big : Point { pub fu area() { return this.x * this.y; } }
let b = Big(5, 6);
print b.area();      // 30
print b + Big(1, 1); // operator dispatch, inheritance-aware
print b as str;      // the conversion door
```

**the machinery to review**:

- **frozen shape (C15)**: a class body defines fields and methods at
  runtime through OP_DEFINE_* ops; OP_CLASS_END freezes the layout - the
  flat field map (name → absolute slot, built parent-segments-first), the
  method tables. after freeze, definitions are refused forever. this is
  what makes raw `method_entry*` pointers inside caches sound.
- **copy-down single inheritance**: the field map is flat - a subclass's
  `find_field` is one probe, the parent chain walked once at freeze, not
  per access. redeclaration anywhere in the chain is a hard error at
  definition time (no shadowing, no quiet narrowing).
- **the member ring (the thing patch 51 opened)**: methods and fields are
  private by default, visible inside the defining class and its
  subclasses (`closure->owner is_a defining segment`). `pub` opens a
  member to everyone, module boundaries included. the ring is enforced
  at the five VM access lanes, not by the compiler - a script subclass of
  a C class sees inherited fields through the same ring.
- **statics**: a second method table (receiver = the class value) and a
  per-own static field store (`static_values`, never copied down; lookup
  walks to the defining class). `static let mut` is writable through the
  class name from inside the hierarchy; `pub static let` (patch 51) opens
  it outside.
- **operators**: the overloadable surface is a fixed set (arith, compare,
  negate, subscript get/set, call) spelled as canonical method names
  ("operator +" etc.), dispatched through a **class-resident operator
  cache** - one `method_entry*` slot per (class, operator), filled lazily
  on first dispatch, sound forever because tables freeze (C15). arity is
  checked at dispatch; unary/binary minus split by arity.
- **conversions (`as`)**: total, throwing, rust-like precedence. `x as
  num/str/bool/map/array` and `v is T` / `classof(v)`. the eight builtin
  classes (str, num, bool, array, map, function, nil, coro) are ordinary
  frozen classes with native method tables - "natives as classes" (patch
  34) is the door raylib later walks through: `ok_class_new` +
  `ok_class_define_native_method` + `ok_class_freeze` from C.
- **the copy-on-redefine discipline (patch 35)**: a class body redefined
  (a closure re-run, a local class in a loop) takes a deep copy of its
  member protos and freezes the copy - same parent, identical flattened
  layout, so the bake stays valid. a different parent refuses.
- **the MIC (monomorphic inline cache)**: `a.x` compiles to one
  OP_GET_MEMBER with its own cache slot; the miss resolves
  field-vs-method by name, the hit is a slot load or a bound-method
  build. the cache is per-site, keyed on (klass, recv_is_class).

**the patches**: 30 core, 31 performs (numbers-first dispatch), 32
operator dispatch, 33 conversions + identity (`===`), 34 natives as
classes, 35 copy-on-redefine, 36 a perf pass, 37 the glob-classes fix.
sessions 26-32 in the WORKLOG carry the C1-C18 rulings verbatim.

**what to review hardest**: the ring's edge cases (privacy_fields,
privacy_methods, pub_survives_error - an override of a pub ancestor
method must be pub or refuse), and the freeze walk (okvm.c
`freeze_walk_chunk`): member protos are patched in place per definition,
the `patched_by` marker is the single-walk guard.

---

## §2 — 0.6.0: coroutines (patch 38)

**the model**: stackful, lua-shaped. `let c = coro(fu); c.resume(v);
c.status()` - 0=ready, 1=suspended, 2=running, 3=dead. per-coro register
file + call stack (the same dispatch loop, swapped execution state), so
suspend is a frame swap, not a transformation.

**the rulings that shaped it (R20-R24, COROS-draft.md)**: yield only
from ok frames - the **yield barrier**: a yield under a C-driven region
(operator dispatch, construction, defer unwind, a native call) is refused
with the same error shape, because C frames cannot be parked. R21: a
dead coro answers null on resume. R22: a dead coro's defers run in the
death sweep, not at abandonment. R23/R24: coro is a core native class -
the eighth builtin - which is why it survived the std re-home ruling
("since it is inside the api, it shouldn't be gated in std").

**what to review hardest**: `coro_drive` in okvm.c (the resume driver:
state swap, the parked-resumer chain, error transplant) and the six
call-completion arms that carry the native-suspend flag (§3's patch 42).

---

## §3 — 0.7.0 "Epigone": coro's upper stretch (patches 39-42)

[session 42 correction: this stretch lives in coro's history, not on
any "tidy" branch - Q's tidy stalled at the bare coros commit and is
retired from the release pipeline (runbook round two); coro carries
everything.]

Q's picks, one apply-session, all debts:

- **39 defer continuation**: defer no longer halts the statement's value
  lane - a `defer` in position computes through. the RETRO in session 25's
  record explains the old trap.
- **40 the rename**: `type` became `class`/`Object` on the C side and the
  meta-class names grew up. source-compatible on the script side.
- **41 universal string interning (D20)**: every ok string is canonical -
  the interner is consulted at creation, so `===` on strings is word
  equality (free, no vtable entry), string-keyed maps probe on pointer,
  and the module registry's path keys are interned by construction. the
  interner is a WEAK set: mark never walks it, sweep removes dead entries
  first (a freed canonical would dangle in a probe chain).
- **42 the coro C API (D21, "vm_tick" investigated and answered)**:
  ok_coro_new / ok_coro_resume / ok_coro_status / ok_coro_current +
  **ok_native_suspend** - a native requests its CALLER's suspension at
  the native-return boundary (the yield barrier still guards it; six
  call-completion arms carry the flag). this is the game-loop primitive:
  `while (running) { ok_coro_resume(...); render(); }` - no trampoline
  closures. it landed BEFORE the module ABI so the vtable would inherit
  it instead of breaking later.

**what to review hardest**: D20's weak-set sweep order (sweep_interner
runs before the object walk) and the suspend flag's validation
(owner coro, native depth, barrier) that guarantees no stale flag.

---

## §4 — 0.8.0 "O Resolution": modules (patches 43-50)

the wave that made oklang standalone. MODULES.md is the complete design
record (the M1-M14 rulings, the four patch scopes, the amendments); this
is the tour.

### 4.1 the spine (OR-1, patches 43+44)

```ok
import "std/io";                      // binds the PATH: std.io.print(...)
import "std/math" as m;               // alias binds the MODULE itself
from "std/str" import upper, split;   // bind exported symbols directly
import "./util.ok";                   // file-relative, raw ok files
import "raylib";                      // the same surface imports natives
```

all compile-time (M4: no importlib in v1, nothing precludes a runtime
lane later). quoted paths only (M1) - bare identifiers are a hard error
that teaches the spelling. `import "a/b"` binds the first segment as a
namespace and merges (M5); `as` rebinds the module itself; `from` binds
pub exports. imports are top-level only. `std` is a reserved root (M8).
a cycle is a hard compile error (M13 - "the cycle is in YOUR design").

the machinery: the compiler resolves the import graph depth-first, each
module is one TU compiled once against the snapshot (D8 carried whole),
keyed by interned canonical path. a module's exports are a **slot array +
name table** - the compile-time globals trick one level up, so
`std.io.print` is a module value load + slot load, zero runtime name
lookup. the module VALUE is a frozen namespace instance (the same class
machinery - `module_namespace` classes whose field reads are public by
doctrine and whose writes are refused: namespaces are sealed). top-level
code runs once, on first import, depth-first order. per-module source
registration keeps file:line:col honest across module stack traces.

### 4.2 the std re-home (OR-2, patch 46)

std/ is a **built-in registry**, never the filesystem: entries keyed
"std/<stem>", built lazily on first import, members harvested from the
natives' hidden globals into the same sealed-namespace shape file modules
get. the shadow test proves OKPATH cannot touch it (M8 has teeth: user
code cannot shadow std because it is never found through the path walk).
the current table: std.array (len, push, pop, remove), std.map (len,
remove, get), std.io (print), std.math (empty, opens with the stdlib
growth version), std.str (empty, same). coro stays a core native, not
std (Q's ruling). **the flat globals stay as deprecated aliases** - the
print statement deprecation and the flat-name removals are the next
stdlib version's calls, one warning release first, per the ruling.

### 4.3 the sandbox + natives + the embedder loader (OR-3, patches 47+48)

three layers, loading born gated:

- **the capability ratchet**: ok_specs grows caps; `ok_caps_drop` only
  ever DROPS (the security property - nothing escalates, not a native,
  not a script, not a manifest). families: NATIVE_LOAD (in-process
  arbitrary code), NATIVE_BUILD (out-of-process), HOST (fs/env/clock as
  they land). CLI: --restrict, --sandbox=locked, --allow/--deny.
  structural denial: a loader whose load_native is NULL denies natives
  by construction, flags or not.
- **ok_module.h, the native ABI**: a native is a shared object exporting
  exactly ONE symbol - `ok_native_export(const ok_api*)`. it never links
  against oklang symbols (no -rdynamic, no collisions, several vms
  coexist); every reach into the vm goes through the **ok_api vtable** -
  static and stateless, stashable for the process lifetime. the table
  carries the whole ok.h surface per Q's "everything we have, exposed to
  the abi" ruling: values (construct AND read), type/class identity,
  calls, the class door (class_new/define_native_method/freeze),
  module_export, errors, roots, the coro primitives, and the field trio.
  deliberately OUT of v1: member ENUMERATION (entangled with the
  visibility round, no in-tree consumer; the append-only discipline
  makes its bump one clean addition later) and subclass creation from
  native.
- **the loader vtable**: the vm and compiler never touch the filesystem
  - the host installs `ok_loader` (read_source + load_native), the CLI's
  default is the fs lane, and the capi test carries a memory-backed
  loader as the embedder example (a full import graph from RAM, zero fs
  calls). dlopen/dlsym hide behind compile-time wrappers (LoadLibrary on
  windows, dlopen everywhere posix-ish, host hooks otherwise).

allocation discipline (patch 47, Q's review): the whole module spine
runs through the user's allocators - the loader contract included. the
registry read api (also patch 47): ok.h grows the loaded list, each
module's import edges, and any module value's export enumeration -
"otherwise how is cli gonna even read the exported table?"

### 4.4 the field trio (patch 49) and project.ok (patch 50, OR-4)

patch 49 answered Q's state question scoped: per-instance STATE needed
the access half - `ok_class_add_field` + `ok_value_get/set_field`, the
data plane is vm field slots, gc-traced, no getter round-trips through C
storage. the C lane is trusted (the ring is script doctrine), mut is
honored, module namespaces refuse writes. the native_state fixture is
the proof: a Counter with payload/step fields, script subclass and all.

patch 50 is the project shape. **project.ok is strictly ok** - a plain
module evaluated in a fresh instance with the full stdlib and NO natives
(M10), exporting one pub table:

```ok
pub let project = {
  "name": "myapp",
  "version": "0.1.0",
  "ok": "0.8",
  "modules": ["src"],                 // import roots, default ["src"]
  "deps": {                           // path deps only (M11): no registry,
    "mathx":   {"path": "../mathx"},  // no downloads. git clone IS the
    "helpers": {"path": "vendor/helpers"},  // dependency manager
  },
  "natives": {
    "raylib": {
      "dir": "native/raylib",
      "build": "make -C native/raylib",          // whatever toolchain (M12)
      "artifact": "native/raylib/bin/raylib.so"  // what "raylib" resolves to
    }
  }
};
```

discovery walks UP from the ENTRY FILE (never cwd - the last implicit
working-dir dependence dies there) to the nearest project.ok; that root
governs. the resolution ladder, natives first-class at every step (M6):
relative → std (never the path walk) → the governing project's roots →
its native targets (built if stale and allowed) → its deps by name
(recursing into dep projects) → OKPATH, last. the 404 teaches: the full
trace of every location and form tried. `ok build [--clean|--force]`
runs the declared build commands and stamps .okbuild/<target>.stamp;
auto-build-on-import asks consent the first time (--yes/--no-build, the
tty prompt); a bare script NEVER builds anything. `ok where "mathx"`
prints the resolution trace without running. the fixture farm (14 dirs
now) covers every failure mode: missing deps fail eagerly, dep/native
name collisions name both (M13), cycles name their chain, restricted
mode refuses with the capability error, where404 teaches.

the manifest harvest rides public machinery end to end:
ok_module_evaluate (the standalone lane), ok_map_pair_at (the keyed-table
read the harvest needed), the governing project matched by
deepest-root prefix against the installed graph.

**what to review hardest in the whole wave**: the loader's abi/name
checks and the mount window's rooting (the fn-staging loop guards each
value across its staging append - and patch 51 taught that the
mount-callback lane needed the same guard, see §5), the capability
ratchet's one-way door, and the consent policy's failure modes.

---

## §5 — the two follow-ups (patches 51 + 52, this session)

### 5.1 pub members: fields join the ring (patch 51)

Q's ruling: "yes this what i mean member publicity in scripts (in a way
that does not comprimise the current design)". methods carried `pub`
since Citadel; fields were the gap, and the workaround was getters -
"so we dont have to make getters and setters and lose perf there".

`pub let` / `pub let mut` / `pub static let` now compose on class-body
fields exactly like `pub fu`: readable outside the defining hierarchy
(and writable with mut), module boundaries included, a slot load on both
sides of the module line. private stays the default; the ring is
unchanged; `pub let x` without mut reads from anywhere and writes
nowhere (the mut error, not a visibility error).

the mechanics, for the reviewer: the pub flag rides the instruction
word's spare bit (the R operand - 64-bit word2 had exactly one free bit
after OP/P/A/B/C/Q/CMP), carried by OP_DEFINE_FIELD and
OP_DEFINE_STATIC_FIELD, decoded into `field_entry.pub`, consulted by the
five vm access lanes before the ring check. the C lane grows
`ok_class_add_field_ex` (ok.h + the vtable's appended tail - the one ABI
3 addition); the old entry stays as the private default. the module
namespaces now declare their exports pub explicitly (doctrine made
honest). fixtures: pub_fields, pub_statics (+ both immutable-error
pins), the cross-module pubmembers pair (the importer reads/writes the
exported class's pub fields; the private one refuses), native_pubfield
(a C class with pub x/y read and written from script as slot loads, the
private counter behind the ring), and the capi section (an Open class
driven from C, 39 checks).

two en-route fixes, both caught by the gates, both reproducing on the
patch-50 head (fixes, not regressions):

- **the stamp learns the abi**: .okbuild stamps now record the host's
  native abi line next to the build command. the 2 → 3 bump left a
  green stamp over a stale .so (staleness could not see the toolchain)
  and project_native taught it the hard way. an ok upgrade now
  invalidates native artifacts - one rebuild, honestly owed.
- **the mount guard**: api->module_export now guards the value across
  the staging append. the window between "the C callback holds the
  class" and "the marked mount array holds the class" was always
  collectable; under aggressive gc the swept class's memory got reused
  by the namespace instance before anyone read it back (the export slot
  showed `<module ...>`). the fn-staging loop always had this guard; the
  mount-callback lane did not. patch 51's extra field-array append just
  made the reuse deterministic.

### 5.2 the ABI goes semver-shaped (patch 52)

Q's ask: "since we are approaching v1, that means any bump to the number
in the abi requires a full major update right? this is bad, we have to
make it compatible, like versioning in semver combat way so we dont get
forced to v2 when we just want to incrementally add something right?"

agreed - and the append-only discipline (kept "as a discipline" since
OR-3) had already paid for it:

```c
#define OK_ABI_MAJOR 0u
#define OK_ABI_MINOR 3u
#define OK_NATIVE_ABI ((OK_ABI_MAJOR << 16) | OK_ABI_MINOR)
```

- **major** breaks: reshuffled vtable, changed signature, moved field,
  renumbered tag → the rebuild message.
- **minor** is additive only: appended vtable entries, appended
  descriptor tails. the loader takes any module with the same major and
  a minor ≤ the host's; a newer minor refuses with the upgrade message;
  minor 0 is a zeroed descriptor, a rebuild.
- the encoding keeps history intact: abi 1/2/3 decode as (0, n) and
  load. v1.0.0 freezes (1, 0) and the compat window opens for real:
  **every 1.x ok runs every 1.y module with y ≤ x, and no addition ever
  forces a v2.**
- both directions still hold: a module compares p_api->abi against
  OK_NATIVE_ABI exactly as before, or decodes the pair for a softer
  stance.

the proof runs on every gate now: **abiminor_old** is a fixture module
that hardcodes `.abi = 2` (frozen in the patch-49 era) - it loads and
runs unchanged on the (0,3) host. badmajor declares (1, 0) and takes the
rebuild message; badabi's 99 is now the newer-than-host lane.

---

## §6 — the language surface today (a script author's one-screen)

values: nil, bool, num (f64), str, array, map, function, class,
instance, coro, native. bindings: `let` (immutable), `let mut`, `glob`
(process-global escape hatch), spans/destructuring
(`let (x, y) = f()`), varargs + spread, default args. control: C-style
for, numeric-for, for-in, while, if/else, the conditional expression.
errors: `throw` any value, `try/catch`, errors as values, defer (with
continuation). classes: §1. coros: §2. modules: §4. members: pub or the
ring. strings are ascii v1 (utf8 is 0.9.0), `===` is identity,
comparisons are throwing-total via `as`. printing: the statement and
std.io.print, %g numbers (the shortest-exact form is the deprecation
window's business). the REPL is `ok` with no arguments; imports persist
across lines through the module registry.

## §7 — the C surface today

**ok.h (static embedders)**: ok_create(specs) / ok_run / ok_call /
ok_free; globals (define/get, native functions); values (construct +
read + the as/is family); classes (new, define native method, freeze,
add field - private default - and the ex pub form); fields (get/set);
coros (new/resume/status/current, native_suspend); roots (ok_root/
unroot); print hooks; the error lane (ok_last_error); the module read
api (loaded list, import edges, export enumeration); module_where;
module_evaluate (the manifest lane); caps (get/drop). pure C99, pinned
by a build (the capi harness, 324 checks x5).

**ok_module.h (native module authors)**: the one-symbol export, the
descriptor (abi, name, version, fns, mount_classes, on_load/unload),
the ok_api vtable (§4.3), the dylib hooks (host-provided or platform),
the loader vtable, ok_native_force_load. the versioning story is §5.2.
the sample shape to copy: tests/e2e/tests/modules/native_hello/ (one
function, one class, one constant, on_load) and native_state/ (fields +
init + methods). build is whatever toolchain - `gcc -shared -fPIC
-Iinclude -o m.so m.c`.

## §8 — std and std-zero, the current state (Q's question, in the record)

**std-zero** (the always-loaded half): the eight builtin classes
(str/num/bool/array/map/function/nil/coro) with their native method
tables, the flat function globals (the seed names and method spellings),
and the print statement - all fused into libok, registered at startup,
zero imports needed. this half CANNOT be dlopen'd or path-resolved: it
is the language's bones, needed before any loader exists (including the
manifest instance's "full stdlib, no natives" guarantee - std-zero is
pure compute, capability-untouched even in --sandbox=locked).

**std** (the optional half): the built-in registry of §4.2 - lazy,
sealed namespaces, imported by the std/ path, never found through the
filesystem walk, unshadowable (M8). currently the natives re-homed
(std.array/map/io) plus two empty doors (std.math, std.str) that open
with the 0.9.0 std expansion.

**what ships where (the static-vs-dlopen ruling this guide records)**:
libok stays statically whole - std-zero AND the std registry are
compiled in, because they must exist before loading exists and must
not depend on the capability gates. the dlopen lane exists for exactly
one population: third-party native modules (raylib, user projects).
the 0.9.0 std expansion continues the pattern: hot core in C, leaf
modules as ok source embedded in the registry (in-memory sources
through the ordinary module pipeline - no filesystem, no dlopen, still
fused). nothing of ok's own ever loads through OKPATH.

**per-project overrides**: deps and natives are fully per-project
(project.ok); std is deliberately NOT overridable (M8 - the language's
own surface cannot be swapped under a program). a project that wants
its own "math" names it anything else and imports it normally.

## §9 — what to review hardest (risk-ranked)

1. **the mount window's rooting** (§5.1) - the one lane where the gates
   caught memory corruption. walk okmodules.c's register_native with
   aggressive gc in mind: every value between "C holds it" and "the
   marked array holds it" needs a guard.
2. **the capability ratchet** - ok_caps_drop's one-way door and every
   gate site (the loader's native lane, the build hook, the manifest
   instance's caps). the structural denial (NULL load_native) should
   stay redundant, not become the only lock.
3. **the ring's five lanes** (§5.1) - field->pub before the ring check,
   each lane; the module_namespace exemption; the statics pair.
4. **the ABI compat rule** (§5.2) - the decode, the two messages, the
   minor-0 guard; the fossil fixture keeps it honest forever.
5. **the resolution ladder's ordering** (relative → std → project →
   OKPATH) and the 404's teaching trace.
6. **D20's weak interner** under the module registry's path keys - the
   sweep-before-walk order is load-bearing.

## §10 — applying and verifying (the receipts)

on a clean modules-staging at patch-50 (`fcecbaa`):

```
git apply 51-pub-members-fields-join-the-ring.patch
git apply 52-abi-semver-the-compat-window.patch
```

then the matrix (the sandbox scripts): build_ok.sh for each of plain/
threaded/asan/aggressive/asan-aggr/threaded-asan/threaded-asan-aggr,
run_e2e_full.sh per flavor (510/510 each - the fixture build phase
rebuilds every native first), run_unit_tests.sh gcc + asan-aggr (unit +
embed), the capi build+run x5 (324 checks), delta blue correct at
1.40654e7. both patches were verified apply-clean in sequence on the
patch-50 head in a scratch worktree before delivery.

the one known debt this guide leaves on record: tree-sitter-oklang needs
the pub-on-class-body-let grammar catch-up (Q's flank, the patch-44
supersession precedent - the sandbox carries no grammar to regen).
