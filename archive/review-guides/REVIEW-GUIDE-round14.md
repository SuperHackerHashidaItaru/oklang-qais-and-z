# round 14 — the no-question branch: unbreak + design review

session 13.1. owner: "i decided to add some features to the language, and
'fix' some stuff... it just broke the whole thing!!... take a look at them,
and like see whats the issue cuz i got bored mid way lol."

scope: `no-question` branch @ `0a88601` (5 commits on master `3382504`),
`design.txt` (4 design items + 1 known issue), 86-file test sweep to the
new syntax, updated collab bench sources.

result: **the branch was ~90% done.** the design is sound, the machinery is
almost entirely built, and the breakage was 7 small defects — mostly the
kind that happen when you stop mid-rewrite. after `09-no-question-unbreak.patch`
(their fix is 43 insertions across 6 files): **e2e 210/211, the one failure
is a deliberate design decision left for the owner** (see §4).

---

## 1. what was actually broken (root causes, in discovery order)

| # | defect | file:line (pre-patch) | symptom |
|---|--------|------------------------|---------|
| 1 | `string_deinit(p_vm->alloc, &fstr)` — args swapped | okvm.c:988 | **does not compile.** the only build error on the branch |
| 2 | `e_LARGE_MASK 0x10` vs `e_LARGE_LOFFSET 16` — encoder ANDs bit-16 shifts with a bit-4 mask; `encode_oppAe`'s `p_e` is `bool` (truncates 2..6 → 0/1) | okchunk.h:252-254, :318 | **every `return <expr>` in every program returns null.** 65/206 e2e failures from this alone |
| 3 | hidden-scope loop commented out (with a stale missing `(ast_node*)` cast) | okcompiler.c:500-507 | `undefined variable: len/push/pop/remove` — all arrays/defaults tests |
| 4 | `add_local` ignores its `p_initialized` param; `resolve_local` skips UNINITIALIZED-flagged registers | okcompiler.c:289-295, okcompiler.c:960-964 | even with #3 revived, core names don't resolve. note: owner already used the exact flag-clear pattern at okcompiler.c:2771-2773 for the for-in loop var — the fix generalizes his own move |
| 5 | `operator_type_from_token_type` has no `TOKEN_AS` case → `OPERATOR_UNKNOWN` | okoperator.c:30 | `as` aborts the compiler: `assertion '0' failed. invalid binary operator.` |
| 6 | `as_bool` missing `else` — unconditional `*p_result = true` tail | okvm.c:1154-1162 | `null as bool` → `true` |
| 7 | `PREC_AS` placed between SHIFT and SUM — **looser** than `+` | okparser.h:23 | `i as str + " " + len as str` parses as `i as (str + " " + len) as str` → runtime error. contradicts design.txt §1B which requires `(i as str) + " " + (len as str)` |

plus two non-breaking correctness finds folded into the patch:

- **opcode-name drift**: enum inserted `OP_AS`/`OP_AS_LONG` between
  `OP_MODULUS` and `OP_MODULUS_LONG` (okchunk.h:180-182) but
  `OPCODE_NAMES` had them after `LESS_EQUAL_LONG` — every disassembly name
  from MODULUS_LONG on was off by two (`AS` printed as `MODULUS_LONG`).
  the count-only static assert can't catch order drift.
- **operator/opcode type confusion**:
  `p_binexpr->_operator == OP_AS` compared an ast operator against a VM
  opcode (okcompiler.c:2186) — now `== OPERATOR_BIN_AS`. effect of the old
  form: the direct-global emission for the type tag never fired, `as`
  always paid a materialize+LOAD_GLOBAL.
- **disassembler `as` operand**: the generic binop printer treats `p` as
  "constant operand"; for `OP_AS` `p` means global (type tags). this was
  the branch's own `known issues: debug assumes constant, now as operator
  uses g, fix!` — fixed in both narrow and long arms.

the fix for #2 in one line each:

```c
#define e_LARGE_MASK  UINT32_C(0x07000000) // 3 bits, returned enum is 0..6
#define e_LARGE_LOFFSET c_LOFFSET          // bits 24..26, clear of A (8..23)
```

(the `bool p_e` param became `uint8_t` — the enum has 7 values.)

the fix for #7 is one enum move: `PREC_AS` now sits between `PREC_EXPONENT`
and `PREC_PREFIX` — rust's placement (cast level: tighter than `*`, looser
than unary). `i as num * 2` → `(i as num) * 2`, `not x as bool` →
`(not x) as bool`. owner can veto by moving it back; note the design.txt
§1B example pins it above SUM at minimum.

## 2. what the branch actually contains (and how good it is)

for a "half finished" bored-during-a-blackout branch, the state is
remarkable. tracing the four design items:

### §2 dropping `?` + single-statement bodies — fully done, works

- lexer/token: `?` token gone from TOKEN_LIST_X (oktoken.h lost 5 lines).
- parser: if/while/for-in/numeric-for/c-for all demand `{` now; bodies are
  compound statements only, swift-style. the diff is mechanical and clean.
- **the reasoning in design.txt is correct and well-evidenced**: the `?`
  read fine for `if is_nice?` booleans, degraded with c-style for, and —
  the killer argument — every LLM shown a snippet read `?` as a
  boolean-assertion operator, not a terminator. when your language's
  syntax confuses the machines that write most code today, that is real
  usability data, not vibes. aligning bodies with `fu` (compound-only)
  removes a whole error class (dangling-single-statement) and one grammar
  production. dropping it also frees `?` for something better later
  (optional-chaining / error-propagation both want it).
- 86 test files swept by hand. that is the un-fun 80% of a syntax change
  and it is all done.

### §3 conversions + ruby truthiness — fully done, works

- `false`/`null` falsy, everything else truthy: verified (`0`, `""`, `[]`
  all truthy; conditions and `as bool` agree).
- explicitness: `arr as num` → clean runtime error "array is not
  convertable to number." no shady length-coercion.
- `as_bool` had the missing-else (bug #6) — after the fix the whole matrix
  is consistent.

### §4 TU-level core declarations + hidden scope — designed, wired, one decision left

the full chain, which i traced end-to-end before touching anything:

- `ok_run` passes `compile_specs.top_level_declarations` (7 visible names:
  len/push/pop/remove/str/num/bool) and `interpret_specs.top_level_values`
  (the values of the 7 space-prefixed globals).
- the compiler declares them in a hidden scope inside `<main>`: layout is
  `r0` = the "" slot (the call register), `r1..r7` = the core natives +
  type tags, user code starts at `r8`.
- the VM stages them in `vm_interpret` at `hidden_start + i` — written
  **after** `call()` nulls the frame window, so ordering is correct, and
  `[0, top)` rooting keeps them alive for the whole program (they are
  double-rooted: registers + the space-globals).
- nested functions reach them through the normal upvalue machinery — this
  is why all closure/default/array tests pass once #2..#4 are fixed.
- for-in does **not** use the hidden scope: it still reads the `" len"`
  space-global directly (okcompiler.c:2787). good — the loop intrinsic is
  immune to any shadowing by construction.

my additions to complete it: the hidden loop revived (with the cast), the
user top-level scope added (compile_root's statements were landing in the
hidden scope itself → `redefining local: 'len'` for any top-level
`let len` — this, i'm fairly sure, is the wall that got the block
commented out), and the `p_initialized` wiring. scope stack now:
`outer("") → hidden(core) → user top-level → …`.

the space-prefixed-global stash ("rooted reusable collection") is honestly
flagged quick-and-dirty in design.txt and it is fine as one: it costs 7
global slots, is unlexable, and dies the day modules land for real.

### §1 shadowing semantics — documented, and both examples now pass

- `let str = "hi"; 1 as str` → runtime error "invalid type" ✓ (lexical
  shadowing kills the tag, exactly as written).
- `let len = 22` in a function: user code sees 22, for-in keeps working ✓
  (verified: prints `1.000000 22.000000` …).

## 3. my opinions on the design (the "is it justified, is it good" ask)

**dropping `?`: yes, unambiguously.** it was doing three jobs badly
(terminator + implicit truthiness assertion + single-statement enabler)
and none of them survive contact with the c-style for. the LLM-confusion
argument alone would carry it. this is a young language's biggest
advantage: syntax churn is nearly free now and ruinously expensive after
1.0. spend it.

**compound-only bodies: yes.** it matches `fu`, it kills the
`if x print y` typo class (the one where you add a line below and it's
outside the if), and it makes the grammar uniform. the one thing you lose
— `if x print y;` one-liners — is exactly the thing people misread.

**ruby truthiness: correct call, and the reasoning in design.txt is the
right reasoning.** you considered strict-bool conditions (error on
non-bool) and rejected it because conditions elsewhere already had a
truthiness notion — consistency won. agreed: a language where
`if x {}` and `x as bool` disagree about truth is worse than either
individual choice. one nit: **`0` truthy is the semantically load-bearing
choice** (js/php/python all disagree here); it's now enshrined in a test
(operators/as/basic.ok) so it's a decision, not an accident.

**`as` as an infix operator with type-tag values: good bones, and the
reserved-tag trick is clean.** the type tags as first-class values
(`RESERVED_1_AS_VALUE(TT_STR)`) means `as` needs no special grammar, no
keyword dispatch, and hosts can later inject more types as globals — the
`p`-flag on OP_AS (global-or-register RHS) is already built for that. two
discussion points:

1. **`1 as str` → `"1.000000"`** — currently number→str uses the print
   format. python gives `"1"`, lua gives `"1.0"`, js gives `"1"`. if the
   language ever grows number formatting (`fmt`/`toFixed`), `as str`
   should probably be the short form and print stay the full one — or
   vice versa. not urgent, but it's now visible in tests either way.
2. **`as` on the left of comparisons**: with rust placement,
   `x as num < 5` groups `(x as num) < 5` ✓. with the old placement it
   was `x as (num < 5)` → runtime error. worth keeping the rust placement
   for exactly this reason.

**TU-level prelude: right idea, and the design goal is correctly
identified** (globals are a shared namespace; a TU overriding `len` must
not poison other TUs — python's builtins are a *global* singleton and
that's exactly the failure mode you're avoiding). the
space-globals-are-a-stash honesty is good. see §4 for the one open
decision.

## 4. the one open decision: `glob let len` vs the prelude

the single failing test is `regressions/shadow_len_does_not_invalidate_for_in.ok`
(written on master, expectations carried over). it does:

```ok
glob let len = 67;
for let i : arr { print(i); print(len); }   // expects 67
```

on master this passed: core `len` was a **global** (`"len"` public +
`" len"` intrinsic), so the user's `glob let len` redefined the public
slot and for-in read the intrinsic. on the branch, core `len` is a
**hidden local of `<main>`** — and locals beat globals in resolution, so
bare `len` now always finds the native; the user's global exists but is
unreachable by name in the TU. current output: `<native function len…>`.

three coherent options:

- **(a) keep locals-first (current)** — the prelude is a true lexical
  outermost scope. invariant is strongest: no global write anywhere can
  change what bare `len` means in any TU. cost: `glob let len` becomes a
  silent footgun (write-only name). mitigation: compiler **warning** on
  `glob`-declaring any prelude name — design.txt itself says "there is no
  solution, only warnings."
- **(b) python-builtins order** (user locals → globals → prelude): the
  test passes unchanged. but resolution must then consult globals before
  the prelude *through the upvalue path too* (a nested function's `len`
  must see a user global, not capture the prelude local), which is a
  messier rule, and a global `len` written by TU A is then visible to
  TU B's bare `len` — the exact cross-TU poisoning §4 was designed to
  kill. **i recommend against this.**
- **(c) hard error** on `glob`-declaring a prelude name. strictest; the
  test becomes an error-expectation test.

my recommendation: **(a) + warning**, and update the test to the new
semantics (or rename the global in it to `my_len` to keep testing that
for-in survives an unrelated global). but this is semantics-owning
territory — your call. everything else on the branch is done.

## 5. remaining work to land the branch

1. decide §4, update the one test.
2. `git apply` / cherry-pick `patches/09-no-question-unbreak.patch`
   (43 insertions, 6 files; applies clean on `0a88601`).
3. clang-format the touched lines (my sandbox is gcc-only, no cmake — i
   compiled with `gcc -std=c99 -O2 -Iinclude -Isrc src/*.c cli/src/main.c -lm`;
   gcc's `-Wparentheses` noise in okutils.h/okchunk.h is pre-existing).
4. README still documents `?` syntax in the examples (commit b904c91
   updated the CLI example; README needs the same sweep).
5. `1 as str` formatting decision (§3 discussion point 1) — whenever.
6. e2e additions now in the tree: `operators/as/{basic,errors,
   shadowed_tag_error,precedence_shadow,shadow_call_error}.ok` — the
   `as` operator had zero coverage before.
7. benches: collab `bench/results/` are still the v0.0.5 set except
   loop_counter, whose re-run captured only ok-stack (the register/lua/
   python rows vanished — power cut mid-run?). all 30 no-`?` sources
   parse+run on the fixed binary (i smoke-tested 8, outputs match the
   v0.0.5 shapes: `1e+07`, `5e+13`, `75025`…). my crude sandbox timing
   of loop_counter (~198ms median incl. spawn) vs v0.0.5's register
   (~204ms) shows no regression signal, but that's a sanity check, not a
   bench — full hyperfine re-run post-merge.
8. embedding integration test still passes untouched semantics; worth
   one new case asserting `ok_global_define(" len", …)`-style intrinsics
   don't leak into user name resolution. (optional)

## 6. language future + the game question

kept for the chat (this is a discussion, not a ruling), but the short
version recorded here: **make the game.** the branch, once landed, closes
every syntax-level complaint a game script would hit except imports and
classes — and dorg3's script (session 13.0) already told us classes'
weight: porgs as 4 parallel arrays, 3-globals-per-texture. the template
work (dorg3/REVIEW-dorg3.md §7) is the other half. the game is the
dogfood vehicle that ranks the roadmap by pain, which is the only ranking
that survives contact with reality.

— Z, session 13.1
