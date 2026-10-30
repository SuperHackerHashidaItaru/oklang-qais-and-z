# WEBSITE-PLATFORM.md — the tiny platform on clay + wasm (draft, session 17)

status: conversation artifact. Q asked for the architecture before sleeping ("blank
slate, educate me"). OPEN rulings marked at the bottom. this is a draft for Q to
mark up, not a spec.

## 0. one sentence

the browser is the third embedder of the VM (after the e2e runner and dorg3's
host); the platform is a small oklang library + a C runner + ~80 lines of
handwritten JS glue that turns (state, event) into a fresh HTML string, with
clay doing geometry.

## 1. ownership — who owns what (the whole architecture)

- browser owns: the DOM tree, hit-testing, caret + text editing, font shaping +
  glyph raster, compositing, scrolling, the accessibility tree.
- oklang owns: state, the element description, all logic, routing, palette.
- clay owns: geometry (who is where, how big).
- the boundary: OUT = one HTML string per render; IN = event dispatch + ~six
  query natives. strings and integers both ways — deliberately dumb, same
  philosophy as the tracer boundary. dumb boundaries age well.

we are NOT reimplementing an OS. we're using one, and skipping its worst part
(cascade layout) by owning geometry via clay.

## 2. the web in systems terms (the education section)

- DOM = a retained-mode scene graph you patch. browser diffs + composites for you.
- everything hangs off ONE event loop. our wasm export `ok_event(...)` is just
  another listener on that loop.
- event delegation: ONE listener at the document root; `e.target` tells you the
  element; `data-ok-id` maps it to an oklang handler. no per-element listener
  bookkeeping, ever.
- innerHTML = the parser as a patcher: assign a string, browser rebuilds the
  subtree. a few hundred nodes ≈ ~a millisecond.
- F12 console = the e2e runner of the web (printf debugging, fully sanctioned).
- remote debugging from the phone: chrome://inspect over USB — the site is
  debuggable from bed.

## 3. the bridge — complete v1 web API surface (~6 functions, that's the
"minimal but functional" proof)

exports (wasm, called by glue):
- `ok_boot()` — run the site program, first render
- `ok_event(id, type, a, b)` — delegated click/input/submit/key/change
- `ok_render_path(hash)` — route change
- `ok_play_run(src)` — /play: compile + run + capture (print hook, patch 14)

imports (glue, provided to wasm):
- `okjs_measure_text(str, font, size) -> width` — via offscreen canvas 2d
- `okjs_viewport() -> w, h`
- `okjs_prefers_dark() -> bool`
- `okjs_hash() -> string`
- later, itch-driven: okjs_get_value(id), okjs_localstorage_get/set, okjs_fetch

glue (ALL handwritten JS, one file, ~80 lines, vendored forever):
module boot ~10, delegation ~25, measureText ~8, viewport/scheme/hash ~15,
/play wiring ~20. rule: NO LOGIC IN GLUE. every line is a shim from browser
convention to an oklang call. written once, then forgotten.

emcc generates its own wasm-loader JS automatically — none of that is ours.

## 4. programming model

- ONE translation unit (imports are 0.3.0; platform + site share a TU until
  then — keeps the platform honest, no accidental module system).
- the model: module `let state` (already GC-rooted via the globals store) +
  `fu view(state) -> elements` + `fu on_msg(state, msg) -> state`.
  runner loop: event -> on_msg -> view -> clay -> emitter -> innerHTML.
- NO diffing. no vdom, no keys, no reconciliation. v1 re-emits the page;
  hundreds of elements is ~ms. the day it's slow is the day we build region
  patching — not one day before.
- inputs are DOM-owned (uncontrolled): textarea/input values live in the DOM;
  oklang reads them via okjs_get_value(id) when events fire. controlled-input
  reconciliation = won't.
- scroll rule v1: only the page (body) scrolls. inner scrollers + full re-emit
  would reset their scroll — so: no inner scrollers until region patching.
- responsive = code: `fu view(s, w)` branches on width; clay relayouts on
  resize event. media queries are width checks we own.
- dark mode = palette selection: ask okjs_prefers_dark() at boot, pick palette
  data in oklang. theming is data, not cascade.

## 5. clay integration

- vendored + frozen (staleness ruled fine, session 14).
- clay needs exactly one thing from us: MeasureText(font, size -> width) — the
  one thing it cannot do without a font backend. ours is the browser's canvas
  2d measureText, so layout matches what actually gets painted. cache
  measurements keyed (string, font, size) — note: this cache wants maps.
  maps-itch counter: /play examples #1, measure cache #2, content-as-data #3.
- fonts v1: system stack via the fixed stylesheet; a font table = (family,
  size) pairs registered at boot. bundled woff2 later if wanted.
- on-demand layout: clay runs per render (event / route / resize), never 60fps.
  clay is designed for 120hz; per-event is trivial for it.
- OPEN (s2 decides): does the emitter walk (a) our description tree with clay
  boxes attached — semantic tags from day 1 (button/a/p/h1 + data-ok-id), or
  (b) bare clay render commands — v1 anonymous divs, v2 semantic. leaning (a);
  the unknown is how cheaply clay gives per-element boxes back (command .id /
  userData — one indirection either way).

## 6. juice, honestly

- hover / press / focus: FREE. CSS pseudo-classes on persistent elements,
  browser-managed, zero oklang work, zero per-frame cost.
- entrance effects: CSS animations run on element insertion; `data-anim`
  attribute + tiny stylesheet rules = fades/slides on render + route changes.
- layout animation (element moves A->B): needs persistent nodes + property
  patching = region patching, later. won't do now.
- 60fps anything: dorg3's lane (canvas/webgl). this platform is
  retain-and-patch. won't.

## 7. can / can't / won't

CAN (v1-ish): layout; real text; buttons/links/forms-lite; hash routing;
state + re-render; hover/focus/entrance juice; /play with corpus examples
(// expect: lines = self-verifying examples for free); print-hook capture;
responsive-by-code; dark-by-data; deployment = static files on pages.

CAN LATER (the escape hatches that keep it from annoying you):
- prerender-at-build + hydrate: the emitter is just a function — run it with
  the NATIVE CLI at build time, ship real HTML for crawlers + no-JS phones,
  wasm hydrates on load. because we emit DOM (not canvas), this is nearly free
  and fixes SEO properly.
- localStorage persistence; fetch for content/assets; region patching;
  CodeMirror 6 as a vendored single file; pushState pretty URLs;
  share-via-URL-hash playground snippets.

CAN'T (physics): pixel-identical glyphs (browser owns raster — which is the
feature: crisp native text); guaranteed SEO on JS-rendered hash pages
(prerender is the fix); caret/focus survival through full re-emit (hence
inputs-are-DOM-owned); inner-scroll survival through full re-emit (hence
page-scroll-only v1); server anything (contact = a mailto link).

WON'T (taste): vdom/diffing; CSS cascade/selectors/stylesheets-as-a-system
(one fixed ~30-line stylesheet: reset + font stack + pseudo-class juice);
npm/node/bundlers/mascots; a server; third-party JS (analytics etc.); the
browser-API firehose (natives arrive as itches, one at a time, like language
features); building an editor (textarea now, CodeMirror later, never ours).

the platform grows the way oklang grows: the site itches, the platform
scratches.

## 8. repo placement

OPEN -> my call: `website/` in the oklang repo (monorepo).
why: it IS the acceptance test for the embedding API (the stranger embedding
oklang is the browser); CI can build the wasm artifact per commit later; zero
version skew between platform and VM during 0.x breaking changes; it forces
the platform to track master reality. graduate to its own repo when a second
consumer appears (tree-sitter precedent).

    website/
      runner/        C++ boot: VM, natives, clay, exports (embedding-test shape)
      platform/      oklang lib: ui builders, clay sugar, router, state helpers
      site/          the actual site program (pages, content, palette)
      vendor/clay/   frozen
      glue/boot.js   ~80 lines, the only handwritten JS
      shell/index.html
      build.sh       emcc (dorg3's flags as the starting point)

counter-consideration: clone weight — vendored clay is hundreds of KB, fine;
emsdk is a tool, not a vendored dep.

## 9. the couple of days

- evening 0 (an hour; already done once for dorg3): s0 — steal dorg3's WASM
  flags + template.html, emcc the VM, hello.ok in a browser. pre-empted
  gotcha: file:// refuses to load wasm (CORS) — `python3 -m http.server`.
- day 1: s1 — re-run the nan-boxing alias tests under wasm32 (dorg3 running
  fine = one heap layout, not proof; strict macros = the net; the test proves
  the net). s2 — vendor clay, bind one laid-out box from oklang, dump the
  command array. THIS IS THE ROOTING API'S ACCEPTANCE TEST: string_bag was
  the rehearsal, clay is the customer.
- day 2: emitter + boot glue + event delegation -> THE COUNTER: an oklang
  button, clicked, state flipped, page re-rendered, in a browser. opening
  night of the whole native boundary: callback -> state -> re-render ->
  tracer marks state.
- day 3: hash router; /play (textarea + print hook + corpus examples +
  fuel/step-limit for runaway loops); palette; deploy to pages (dual, like
  the repos).

each day ends with something visible in a browser — morale by design.

## 10. standing chores (post-v0.1.0)

v0.1.0 tag: DONE (9ad153c, the gate-verified commit — good tagging).
remaining: `gh release create v0.1.0 -R QaisAlsaid/oklang --generate-notes`
(+ backfill v0.0.5 — badge still red), tree-sitter-oklang's own tag now that
16 landed, benches.yml first manual dispatch (also re-baselines outputs
after the %g change).

## OPEN rulings for Q

1. `website/` in the oklang repo (my yes) vs separate repo on day one?
2. emitter shape: tree-walk, semantic-from-day-1 (my lean) vs command-walk
   divs-then-semantic — s2 informs, not blocks.
3. name. placeholder `okweb`; his pattern is dorg/puer/porgs — his call.
