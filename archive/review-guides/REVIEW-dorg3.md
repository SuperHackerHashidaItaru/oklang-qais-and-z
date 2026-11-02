# DORG3 — the deep read: architecture, the resize system, and the road to a template

read by Z, session 13.0 (28/10). every claim below was verified against the source (b32a42a);
oklang-side semantics verified against v0.0.5 (c28d92d). nothing here is speculation — if i
could not confirm it in code, it is marked as a question, not a finding.

scope: full read of all 14 engine sources (~3.8k lines incl. the 2413-line binder), the
1512-line game script, CMake/flake/template.html/README, and the asset pipeline. miniz flag
semantics verified against the vendored source (submodule initialized for this read).

---

## 0. what dorg3 actually is

```
main.cpp ──► app (window, render target, camera, viewport, screen switching)
              │
              ├─ loading_screen ──► load_manager ──► fetcher (curl | emscripten_fetch)
              │                       │  IDBFS seed-vs-persisted manifest dance (web)
              │                       └─ miniz zip extract (traversal-guarded)
              ├─ game_screen ──► binder (85 natives) ──► oklang VM ──► dorg3.ok
              └─ error_screen
```

- **the split is right.** `runner/` never touches game content; `game/` is pure data
  (assets + the .ok script). the engine knows the *protocol* (manifest, callbacks,
  render_config), the game knows the *game*. that is exactly the seam a template needs.
- **the screen abstraction earns its keep**: `init/shutdown/update` + a per-screen
  `render_config` that the *script* can change at runtime (`set_screen_prop`) — including
  filter, logical size, and scale mode. few jam engines get this right.
- **the update pipeline is honest**: `loading → ready/failed` state machine, every failure
  path lands on a real error screen with a real message, the fetcher's progress drives the
  loading UI. the web persistence dance (seed game/ in MEMFS vs IDBFS, version-compare,
  copy-forward-keep-`data/`) is genuinely more careful than most commercial web games.
- the game itself is a single 1512-line oklang file using `glob` state + fork arrays +
  upvalues — and it shipped, on desktop and web, with hot-updatable assets. that is the
  dogfood milestone the whole register rewrite was for. it held.

verdict up front: **this is a legitimate template seed.** the bones are good; the work
needed is (a) a handful of real bugs, (b) closing resize sharp edges, (c) consolidating
the game identity out of the engine, (d) eventually binder-2 once oklang grows the
post-halloween features. none of it threatens the oklang line of work — the opposite:
section 6 is a prioritized wishlist *from* this codebase.

---

## 1. the resize system (the thing you struggled with)

### how it works today — the full path

1. each screen owns a `render_config` `{width, height, scale_mode, filter}` — logical
   resolution, e.g. game defaults to **320×180 POINT** (!) then dorg3.ok's `init()`
   upgrades itself to 1280×720 letterbox linear.
2. `app` keeps three derived things: a **render texture** sized `config × m_render_scale`
   (i.e. at *window* resolution), a **Camera2D** with `zoom = m_render_scale`, and a
   **viewport rectangle** (centered, window coords).
3. scale mode picks the scale: `letterbox = min(wx/cw, hy/ch)`,
   `preserve_width = wx/cw`, `preserve_height = hy/ch`.
4. the frame: game draws into the render texture *in logical coords* (camera does the
   ×scale), then one `DrawTexturePro` blits texture→viewport with a negative source
   height (the raylib FBO flip). because the texture is sized to the viewport, the blit
   is 1:1 — no resampling at the final step.
5. input inverts the same transform: `window_to_logical` = (pos − viewport.xy) / scale.
6. resize = recompute scale + viewport + camera, and **reallocate the render texture**.

this is the correct architecture — fixed logical coordinates, one authoritive transform,
exactly one scaling blit, input and rendering share the mapping. the pain was never the
design; it's the five sharp edges below.

### the sharp edges (each verified)

**E1 — config changes don't apply until the next window resize.**
`set_screen_prop` → `screen->set_render_config(conf)` just stores the struct. nothing
re-derives scale/viewport/render-target until `IsWindowResized()` fires in some later
frame. if the script changes config outside of init — a settings screen switching
letterbox↔preserve_width, a resolution dropdown — the new mode sits latent. worse,
**the mouse desyncs immediately**: `window_to_logical` divides `m_viewport.width`
(old) by `config.width` (new), so input mapping is wrong until a resize "activates" the
change. dorg3 only sets config in `init()`, and `app::invalidate_screen` re-derives
everything right after `init()` returns — which is why you never saw this. it is a
landmine the template would have stepped on.
*fix: `app::refresh_display()` (invalidate viewport+target+camera), called from
`game_screen::set_render_config`. three lines plus the declaration.*

**E2 — render texture reallocation on every frame of a drag.**
`IsWindowResized()` is true for every frame the size keeps changing, so a continuous
drag = unload+reload a GPU texture per frame. it works, but it's the classic source of
resize jank (and on web, each step is a realloc inside the browser's frame budget).
*fix: coalesce — only realloc when the size actually differs from the allocated size by
≥1px since the last realloc, or debounce ~100ms; rendering can keep using the old target
one frame longer while the viewport already updated.*

**E3 — fractional scale × POINT filter = uneven pixels.**
`preserve_width` on a 1366×768 window gives scale 1.067 — a "1px" sprite pixel renders
as 1 or 2 physical px depending on where it lands: the classic pixel-art shimmer. the
system has no integer-scale option. (dorg3 sidesteps it by using linear filtering —
which is why it looks right but *not* chunky.)
*fix for the template: add `letterbox_int` (snap scale to floor, accept the extra
black bars). two lines in `calculate_viewport` + one enum entry. games that want crisp
pixels opt in; games that want smooth keep today's modes.*

**E4 — web canvas is CSS-letterboxed AND engine-letterboxed.**
`template.html` pins the canvas to 16:9 (`min(100vw, 177.7778vh)`), and then the engine
letterboxes again inside it. double letterboxing is invisible when both agree (16:9
config), but a non-16:9 logical size gets bars inside bars. also: the canvas *backing
store* vs CSS size on high-DPI screens (retina, browser zoom, phone) is the usual source
of "why is it blurry" — worth one explicit test on a phone before templating.
*fix: pick one letterboxer. my vote: let the engine do it (it already knows the config),
make the canvas plain `width:100vw;height:100vh`, and let the black bars be the page
background. that also makes non-16:9 games work on the web for free.*

**E5 — desktop HiDPI.** `FLAG_WINDOW_HIGHDPI` is never set; on macOS retina the window
renders at logical pixels and the OS upscales = soft. one `SetWindowState` call + a test.

### one non-bug that looks like one

`app::invalidate_screen` does `m_main_screen = std::move(m_next_screen)` then checks
`if (m_next_screen != nullptr) return;` — looks dead (a moved-from unique_ptr is null),
but the check happens *after* `init()`, and error paths inside `game_screen::init()` call
`switch_screen(error_screen)` — so it is a live guard for "init switched screens again;
defer re-derivation to next frame". it is load-bearing and deserves a comment, because
the next person to "clean it up" breaks error handling during load.

### the resize report card

design: right. input mapping: right (modulo E1). web: works, double-letterboxed. what
made it *feel* hard, i'd bet: E1 (changes that don't take), E2 (jank while dragging), and
E3/E4 (crispness/blur mysteries that are actually filter+DPR interactions). all five are
small, contained fixes — none touch the architecture.

---

## 2. confirmed bugs (verified, with locations)

1. **`web_fetcher.cpp:9-14` — unseeded rng + dead cache-buster.** `std::mt19937 s_mt;`
   is default-constructed (fixed seed 5489); `s_rd` exists but is never used to seed it;
   and `cache_bust()` is *never called* — `fetch()` passes `p_url` straight through. so
   web caching of `manifest.txt`/`dorg3.zip` is fully governed by response headers. if
   codeberg pages sends long-lived cache headers, players can get stale assets.
   *fix: `std::mt19937 s_mt{std::random_device{}()};` and actually append the bust param
   to the two upstream URLs on web.* (contrast: `binder.cpp:1890` seeds correctly — this
   was an oversight, not a choice.)
2. **`binder.cpp` ×3 — missing `return` after `ok_native_error`** (`number_from_str`
   1833-35, `load_file` 1847-49, `save_to_file` 1859-64). verified against oklang v0.0.5:
   `ok_native_error` → `vm_native_error` (okvm.c:91) only *sets a flag* and returns — the
   native keeps executing. so a script calling `load_file(42)` falls through to
   `ok_value_as_string` on a number and dereferences garbage. a script typo becomes a
   native crash instead of a clean oklang error. *fix: add `return ok_value_null();` in
   the three type-check branches.* (language note: the post-halloween error model should
   make "natives must stop after error" unbreakable — see §4.)
3. **`game_screen.cpp:155` — `delete` on a `new char[]` buffer.** the manifest is read
   via `new (std::nothrow) char[fsz + 1]` and freed with scalar `delete`. formally UB;
   the sibling error paths in the same function correctly use `delete[]`. *one-character
   fix.*
4. **`binder.cpp:1855` — `delete` on raylib-owned memory.** `LoadFileText` allocates
   with raylib's allocator (malloc); free it with `MemFree()`, not delete.
5. **`binder.cpp` Vector2 destructor lambdas cast to `native_wrapper<Texture2D>*`**
   (texture_size, get_logical_mouse, get_logical_pointer, …). works today (magic sits at
   offset 0 for every wrapper, and the types are trivial), but it is a wrong-type delete
   waiting for a non-trivial T. hygiene fix alongside binder-2.
6. **runtime artifact `high_score.txt` is tracked in git** — and it is the *old* path;
   the script now saves to `game/data/high_score.txt` (correctly IDBFS-persisted on web).
   remove from the index, add to .gitignore.
7. **`template.html` asset paths.** `loading_bird.png` (css) and `favicon.png` are
   referenced relative to the html, but they live in `game/statics/` — the embed only
   mounts the `game/` tree. unless the itch upload manually adds these files next to the
   html, the loading screen background silently 404s. (worth one look at the deployed
   page's network tab; if so, `--preload-file` them beside the html.)
8. **registration/table nits** (all copy-paste class): `to_str` is registered **twice**
   (2265 and 2279); the `is_font_valid` failure message says `get_default_font` (2012);
   `str_concat`'s second-arg error says "1st" (1784); `atan` has arity 2 and computes
   `atan2` — rename before it fossilizes (scripts depend on it, so rename early in the
   template's life, not later).
9. **`load_manager` CWD contract.** every path (`game/assets`, `./manifest.tmp.txt`,
   `game/data`) is relative to the current directory. fine when launched from the repo
   root (and on web, where CWD is the FS root); breaks for double-clicked binaries and
   .desktop launchers. the template should either resolve assets relative to the
   executable or document the launcher contract loudly.
10. **`fetcher.hpp` `m_error` is a plain string written on the worker thread** and read
    from main. safe in the current usage (read strictly after the atomic `m_failed`
    flag, which orders the writes), but fragile under refactor. one-line comment or a
    small mutex; not urgent.
11. **`load_manager.cpp:304` — `MZ_ZIP_FLAG_COMPRESSED_DATA` passed to
    `mz_zip_reader_init_file_v2`.** verified against vendored miniz: nothing ever
    consults the stored flags on the extract path (`mz_zip_reader_extract_to_file`'s own
    flags parameter governs, and you pass 0). so it is a no-op — but a *misleading* one.
    delete it before someone "fixes" extraction around it.

---

## 3. the binder: what dies, what stays, and the design for binder-2

### obsolescence map against oklang v0.0.5

| binder-1 family | verdict | why |
|---|---|---|
| `modulo` | **delete** | language `%` is native (and floor-correct) now; the binder version casts through uint64 and is wrong for negatives |
| `create_array`/`array_*` (8 fns) | **delete** | language arrays hold *every* value type, no primitives-only restriction, no C++ heap indirection |
| `str_concat` | **delete** | `+` does it, and variadically (str_concat is 2-arg) |
| `to_str` / `to_str_no_decimal` | **replace** | with a real `format`/`to_fixed` once stdlib lands (see §6) |
| `min`/`max`/`clamp`/`exp` (script-side!) | **promote to stdlib** | jam games should not hand-roll these |
| everything raylib-shaped | **stays** | this is the actual binding surface |

net: ~13 of 85 natives disappear outright, plus ~30 lines of script helpers per game.

### the shape problem

2413 lines, of which roughly 1700 are four repeated idioms: per-argument type validation
blocks (`draw_texture_pro` alone has 13 consecutive hand-written checks), wrapper
cast + magic check, registration boilerplate (4 lines × 85), and per-wrapper destructor
lambdas. adding one function = ~30 lines of ceremony. that is the tax you pay every jam.

### binder-2 sketch (post-language-features, or minimally now)

```cpp
// one signature string per function; one generic prologue does ALL checking.
// n=number s=string b=bool T=texture F=font S=sound M=music A=array *=any(value)
static const native_def defs[] = {
  {"draw_texture_pro", "T nnnn nnnn nn n", draw_texture_pro},
  {"set_texture_filter", "T e:filter", set_texture_filter},
  {"vector2_distance_sqr", "nnnn", vector2_distance_sqr_scalar},
  // ...
};
// bind(): loop, define with arity = strlen(sig), wrap in a generic trampoline that:
//   1. checks each arg against its sig char (magic-checked for tagged letters)
//   2. extracts into a small typed view array
//   3. calls the fn body, which now contains ONLY logic
```

- estimated size: **~700-800 lines for the same surface**, and new functions become one
  table row + a body.
- the prologue is also where the three missing-return bugs (§2.2) become *impossible* —
  validation returns before any body runs.
- keep the magic numbers (they are a fine poor-man's type tag) but generate the
  destructor once: `template<class T> ok_value wrap_native(ok*, T, uint32_t magic)`.
- **scalar fast paths for hot math**: `circles_collide` currently costs *four* heap
  Vector2 wrappers per call (two created in `key_update` and never used — dead
  allocations — plus two inside `circles_collide` itself), and `get_logical_mouse()`
  allocates a wrapper per poll. offer `vector2_distance_sqr(x1,y1,x2,y2)`-style
  overloads and let scripts keep wrapper values out of per-frame paths. (this stops
  mattering when oklang grows cheap value types — until then it is free GC pressure
  removed.)
- naming pass while the surface is young: `atan`→`atan2`, error strings consistent
  ("expected Nth argument of type 'X'."), and group constants (`screen_prop_*`,
  `key_*`) — these become real namespaces/objects the day maps land, and flat-global
  names that *read* like namespaces make that migration mechanical.

### one binder behavior worth keeping exactly as-is

`array_foreach`'s callback protocol (null=continue, `false`=break, anything
else=early-return value) is a nice little law. if the language later grows a
`foreach(arr, fu)` in stdlib, keep the same three-valued contract — dorg3.ok already
depends on its break semantics.

---

## 4. the GC rooting hole — the most valuable thing dorg3 teaches oklang

three separate places hold ok_values on the C++ side, and **none of them are visible to
the oklang GC**:

1. **fork arrays**: `std::vector<ok_value>` inside a native wrapper. the
   primitives-only guard (`ok_value_is_object`) blocks closures/arrays — but verified
   against `okvalue.h`: **strings (own tag) and native values (TAG_USER) pass the
   check**, and both are GC-heap references. store a string in a fork array, allocate
   during a later `array_foreach` callback, and the string can be collected while the
   vector still points at it. the restriction is not a policy, it is an incomplete
   workaround.
2. **`game_screen::m_callbacks`**: three ok_values (closures) stashed across frames.
   alive today *only because* dorg3.ok declares them `glob fu` — the globals table roots
   them. a script returning local closures instead would dangle on the first
   collection.
3. any future host stash (settings, save-data caches) hits the same wall.

**the ask for the embedding API** (post-halloween, alongside the error model):

- an explicit rooting protocol — `ok_value_retain(ok*, ok_value)` /
  `ok_value_release(ok*, ok_value)` (lua_ref-style registry), for "C++ owns this for a
  while" cases like m_callbacks; and/or
- a **mark/visit callback on native values** — `ok_native_value_new_with_mark(fn)` so
  containers like fork arrays (or any host object holding GC refs) can walk their
  children at collection time. this is the *general* fix and matches the way the
  interesting bugs in oklang have always been "where things overlap".
- and the error-model tie-in: `ok_native_error` flagging + returning invites exactly
  the §2.2 fall-through bug. either document "must return immediately" and enforce it
  in debug builds, or make natives unconditionally stop (longjmp/unwind) — that also
  deletes a whole class of host-side checks.

this is why the review is worth doing before the template hardens: the template's binder
is about to become the *reference embedding* for the language, and these are the API
scars worth designing out at the source.

---

## 5. template-ability audit

### already template-grade

- `runner/` ↔ `game/` split; screen lifecycle; per-screen render configs.
- the entire load/update/hot-deploy pipeline (manifest versioning, codeberg-pages
  upstream, IDBFS persistence preserving `game/data`, zip traversal guard, `skip` arg
  for offline dev). this is the template's crown jewel — document it, don't rebuild it.
- dual input paths (keyboard + virtual joystick) proven on web/mobile.
- error/loading UX that never leaves the player on a black screen.
- nix dev shells for native and web.

### to become "the dorg engine template"

**consolidate the game identity.** today "dorg3" is hardcoded in: `app.hpp` window
title, the upstream base URL + `dorg3.zip` name (load_manager defaults), the script
path `game/assets/scripts/dorg3.ok`, the `--embed-file` rule, the html `<title>`, the
CMake project name. one `game_config.hpp` (or a tiny template substitution script)
should own all of it. this is the difference between "fork and rename 11 spots" and
"instantiate".

**build hygiene** (small, each one verified):
- `cmake_minimum_required(3.10)` → bump (modern cmake warns; 4.x tightens further).
- no default `CMAKE_BUILD_TYPE` → vendor subdirs (incl. **the oklang VM**) build
  unoptimized in fresh checkouts. set Release default or refuse configure.
- web flags review: `-sWASM=0` (why asm.js in 2026? wasm is smaller/faster/universal —
  if this was a raylib workaround, retest), `-sASSERTIONS=1` (strip for release),
  `-sASYNCIFY` (nothing in the C++ visibly awaits — the EM_JS persistence dance is
  callback-based. asyncify costs size and optimization barriers around the interpreter
  loop; measure removal), no `-sALLOW_MEMORY_GROWTH` with 256MB initial (fine, but a
  conscious choice worth a comment).
- `.gitignore`: add `high_score.txt`, untrack the current one.
- `flake.nix` pins nixpkgs via a `/nix/store/...` path — machine-specific by
  construction (the lock's `lastModified: 0` gives it away). switch to
  `github:NixOS/nixpkgs/<rev>` so `nix develop` works for anyone else. if it worked on
  your other machines, it's because the store path happened to exist there.
- README: "dependeinceies" → "dependencies"; the codeberg clone URL is right, the
  github mirror is worth a line since it exists.
- **document the deploy pipeline** (zip layout, manifest bump, codeberg pages push,
  itch packaging) — it's invisible tribal knowledge today, and it's the thing future-you
  most needs from a template.

**resize fixes from §1** (E1–E5) — all small; E1 is the only one that changes behavior
users can feel.

### the honest template-vs-engine question

for jam-scale games, this + oklang *is* a real stack, and the parts worth growing into
"engine" features are exactly the ones the language unlocks next: entity state as
arrays-of-objects (classes), script splitting (imports), clean failure (error model).
until then the template's job is to keep the boring parts — load, loop, resize, deploy,
error UX — frozen while games churn on top. resist building an engine *around* the
current binder; build the template thin and let the language phases thicken it.

---

## 6. what dorg3.ok says back to the language (roadmap, weighted by this codebase)

read as a requirements document, 1512 lines of jam code under deadline:

1. **classes/value types — the #1 ask.** porgs alone are **four parallel arrays**
   (pos_x, pos_y, dir_x, dir_y); every texture is *three globals* (texture, width,
   height). the parallel-array + global-spray pattern is not a style choice, it is the
   language's ceiling made visible. arrays-of-values (v0.0.5) + classes would delete
   roughly a third of this script's plumbing.
2. **stdlib**: `min/max/clamp/abs/exp` hand-rolled at the top of the file; `atan2`;
   `format`/`to_fixed` (the `to_str`/`to_str_no_decimal` pair exists purely because
   `std::to_string` gives 6 decimals); string building beyond 2-arg concat.
3. **indexed iteration**: `array_foreach` + an upvalue-captured `let mut i` (forks,
   line 612) is the script inventing `enumerate`. an indexed loop primitive (or numeric
   for over arrays) kills both the trick and its off-by-one risk on mid-iteration
   mutation.
4. **imports**: one 1512-line file. even two files (game / menus+music) changes jam
   ergonomics. also lets the template ship a reusable `std.ok` per game.
5. **error model**: `set_error(string)` + bool-return protocol is the workaround; the
   real thing (with messages that reach the player screen) also fixes §2.2's class of
   host bugs.
6. **perf, for the game's sake**: dispatch improvements land directly in the game loop;
   for-in over arrays in hot paths; and the wrapper-allocation pressure from §3
   disappears entirely if vectors/colors ever become value types.
7. **two nice surprises to keep**: `glob let` inside a function = runtime global
   definition (used heavily by `load_textures()` — power feature, deserves docs), and
   the upvalue-index trick is live proof closure capture semantics are sound.

---

## 7. action plan

**P0 — before anything (an evening, all mechanical):** §2 bugs 1-4 + 6-8; E1's
`refresh_display()`; untrack high_score.txt. none touch behavior beyond fixing broken
things.

**P1 — the template pass (a day):** game-identity consolidation; CWD contract; flake
pin; cmake bump + Release default; web flags pass (measure ASYNCIFY/WASM=0); E2-E5;
template.html assets; document the deploy pipeline; README typo.

**P2 — binder-2:** after the language grows imports/errors/classes (your instinct to
sequence it after those phases is right — the registry design in §3 is stable either
way, so nothing here blocks). until then, binder-1 minus the dead families (§3 table)
is fine for the next game.

**P3 — language-gated template growth:** classes → entity layer in script; imports →
multi-file games; error model → drop `set_error`; gc rooting API → the fork-array
restriction and the m_callbacks hazard dissolve properly.

none of this conflicts with the oklang line — §4 and §6 *are* the oklang line, sourced
from a shipped game.
