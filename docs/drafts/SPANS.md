# SPANS — 0.3.0 candidate, first feature draft

status: DRAFT for Q's review. carries D6 (DECISIONS.md, the "is the compiler
arch good for it" answer) forward against the code as it stands after maps
(0987b4e + patch 19), and adds what D6 didn't have to decide: the surface
syntax, the static-arity mechanism, the natives/embed story, and the patch
plan. read with ROADMAP-v1.md §0.3.0.

the name: a **span** is a base register + a length. parameter windows, result
windows and varargs windows are all spans — the milestone is really "make the
register file's native shape visible on the surface, carefully."

## 1. what ships (surface)

```ok
fu minmax(a, b) {
  if a < b { return a, b; }
  return b, a;                      // every return path must agree: 2 results
}

let (lo, hi) = minmax(3, 9);        // destructuring let (R1: shape)
print(lo + hi);                     // 12

fu head_rest(arr) {
  if len(arr) == 0 { return null, []; }
  return arr[0], remove_rest(arr);  // ...
}

let (h, r) = head_rest([1, 2, 3]);  // h = 1, r = [2, 3] (shape sketch)

fu sum_all(nums...) {               // varargs (R3: token)
  let mut total = 0;
  for let n : nums {
    total = total + n;
  }
  return total;
}
print sum_all(1, 2, 3, 4);          // 10

let args = [1, 2, 3];
print sum_all(args...);             // spread (R5: defer or ship)
```

- result arity is **static per function**: the compiler counts what every
  `return` path yields, all paths must agree, and call sites destructure
  exactly that many. no annotation syntax — arity is structural, like
  parameter arity (R2).
- single-result functions stay on the RETURN_1 fast path untouched. a call
  whose callee returns 1 and whose caller takes 1 compiles exactly as today.
- multi-result is script functions v1; natives stay single-result (§6).
- `null` is a legal result value; spans are fixed-length, no lua-style
  "adjust to receiver" truncation/padding (R6).

## 2. the core — D6 restated against the code as it is

D6's answer holds and has aged well: **argument windows and return windows
are register ranges, and the VM already speaks ranges.** the evidence, post
-maps:

- `OP_CALL (p, a, b, c)` already reads `r[a] ... = r[a](r[a+1] ... r[a+b])`
  with a result-count operand (`c == 0` = dynamic take-all) — the RETC lane
  exists.
- `OP_RETURN (a', b')` already returns `r[a'] ... r[a'+b'-1]`, and
  `b' == 0` is the to-top passthrough. `OP_LOAD_NULL (A, B)` already
  null-fills a register *range*. `OP_JUMP_IF_ARGC` already reads
  `frame->argc` for the defaults ladder.
- `call_frame->argc` is set by `call()` — the D6 "single change that
  dissolves the argc-from-top wrinkle" is DONE (it landed with defaults).
- the compiler already threads `requested_count` through
  `compile_expression` — `compile_call_expression` currently
  `CRASH_MSG`s on `requested_count > 1` ("unimplemented multi-result").
  the hook is where the doc said it would be.

so multi-return costs: callee emits `OP_RETURN a, n` (the range form, already
implemented) instead of RETURN_1; results land at the callee's base — which
IS the caller's call-window start — **zero copy, destination-passing for
free**. the caller's destructuring `let` copies into named locals once
(after the window is dead) or elides entirely when the bindings can live in
the window registers themselves (see §4).

what does NOT exist yet and this milestone adds: static result arity on
protos (a `results` field next to `arity`/`registers`), the destructuring
surface, compile-time arity checks at call sites, and the varargs pair
(VA_PACK / spread). nothing architectural blocks any of it — D6 called it,
the port proved it.

## 3. the calling convention today (already landed, for reference)

caller: stages callable at r[a], args at r[a+1..], emits
`OP_CALL (a, argc, retc)`. callee: prologue runs the defaults ladder via
OP_JUMP_IF_ARGC reading `frame->argc`; body; epilogue RETURN_1 (fast path)
or OP_RETURN range. `ok_call` (embed side) stages the same window at the top
of the register file and reads r[base] for the single result.

spans changes none of this. it widens the epilogue choice and formalizes the
result count. the RETURN_1 fast path is load-bearing — keep it: caller-
declared arity is how a register VM keeps single-result calls at one
dispatch, the thing lua's stack-adjust semantics couldn't cleanly keep.

## 4. destructuring

R1 asks the shape. D6 sketched `let (x, y) = f()`. the competing form is
`let x, y = f()` — which the maps patch just spent on the for-head
(`for let k, v : m`): a comma list under one `let` means BINDINGS there.
reusing it for call results would make `let a, b = f(), g()` ambiguous and
give the comma two jobs in one expression. parens win: the span is a
*thing* (a window), and `(x, y)` names its slots. mutability per binder:
`let (mut x, y) = f()` — the binding list inside the parens takes the same
modifiers as any binding.

evaluation/compilation:

- `let (x, y) = f();` compiles the call into a temp window (retc = the
  proto's static results), then either (a) declares x, y as locals AT the
  window registers — zero moves, the window becomes the bindings — or (b)
  moves into fresh locals when the window registers are temps the body
  would clobber. (a) is the goal: the destructuring let is then FREE at
  runtime. the register allocator already supports "locals at arbitrary
  registers" (the for-in hidden state works exactly this way).
- the span expression itself (`f()`) must be a call — literals/binops error
  ("a span binding needs a call"). no `let (x, y) = 1, 2;` tuple literals:
  tuples are NOT a value type, spans are a calling-convention fact. if a
  value-level product type is ever wanted it arrives with classes (0.5.0),
  where it can be an object with real identity.
- nested destructuring `let (a, (b, c)) = f();` — REJECTED v1: no function
  returns a span-of-spans (results are registers, not nested). keep the
  grammar flat; revisit never or at classes.
- results can feed expressions only through bindings: `print(minmax(1,2));`
  with results=2 — ERROR (a call in expression position requests exactly 1;
  the compiler asks the proto). this is the strictness doctrine: no silent
  truncation, no lua "take the first".

## 5. varargs + spread

- `fu f(a, b...)`: `b` binds an ARRAY of the trailing args. the trailing
  args already sit in consecutive registers after the fixed params —
  `VA_PACK (a, b, c)` materializes them into a fresh array local
  (`results`-style: prologue op, one allocation per varargs call, v1
  always packs). pack-on-escape window views are a later optimization,
  explicitly not v1.
- arity checks with varargs: `argc >= fixed` (the floor), no ceiling.
  OP_JUMP_IF_ARGC's existing `p` polarity covers the floor test; the
  ceiling check in `call()` learns "last param is varargs" from the proto.
- `...` needs a TOKEN (the lexer has no ellipsis; `.` is one). R3: the
  token is `...` (three dots, one lexeme) unless you want `*b` (the
  asterisk exists but reads as pointer/deref to a C++ eye). this is the
  only lexer change in the milestone.
- spread `g(args...)`: CALL_SPREAD stages the array's elements into the arg
  window (a copy — args are a window by definition) then CALLs with
  argc = fixed + len. **R5: shippable but deferrable** — it's the only
  piece that's pure sugar (`g(args[0], args[1])` exists today, and `push`
  + a loop covers construction). recommend: land it in the same series
  only if the lexer token is already in; else it waits for the std seed.
- varargs + defaults together: `fu f(a, b = 2, c...)` — the defaults
  ladder reads argc, varargs takes the remainder floor-1. allowed v1
  (the ladder already walks positions); one e2e proves the interaction.

## 6. natives, embed, and the ok_call contract

- `ok_native_fn` returns ONE `ok_value` today, and `ok_call` reads one
  result slot. multi-result NATIVES stay out of v1: the host API surface
  (`ok_call`'s single out-param, call_native's result write) is
  single-result by construction and the honest seam is to say so. script
  functions can return spans; hosts get the first result of a span call
  via ok_call with a compile-time-visible note, or wrap.
  - the alternative (ok_native_fn returns a count + writes a window)
    touches every existing native and the whole embed surface for zero
    current consumers. don't pre-build — same ruling as the interning
    question in maps.
- `ok_call` calling a multi-result script function: allowed, returns the
  FIRST result (the window is at the register top; ok_call's contract
  documents "single result, span calls yield the first"). destructuring
  from C is the host staging its own window — post-1.0 embed API itch,
  not v1.
- dorg3 note: the array_foreach-style callback shapes are single-result —
  unaffected.

## 7. implementation plan (patches, the usual rhythm)

**patch A — multi-return + destructuring (the milestone's core)**:
`object_function` gains `uint16_t results` (0 = infer-completed single, set
by the compiler when the function closes); the compiler: every `return a, b`
path checks span-width agreement across the function (compile error "return
span mismatch: 2 here, 1 at line N" — static arity, no annotation);
`compile_let_declaration` learns the paren binding list (window-at-register
binding, the for-in hidden-local trick); `compile_call_expression` requests
retc from the proto (kills the "unimplemented multi-result" CRASH_MSG);
expression-position calls to multi-result functions error. e2e: two-result
swap/minmax, three-result, null results, single-result fast path unchanged
(bytecode-diff test: same opcodes as today), destructuring with mut,
shadowing, closures capturing span-bound locals, error cases (arity
mismatch, non-call span, expression-position truncation), RETURN range
already covered by existing retc tests. gates: the full battery.
embed: ok_call-first-of-span contract test.

**patch B — varargs (+ spread if R5 says ship)**: lexer `...` token;
`fu f(a, b...)` parse (binding modifier-style marker on the last param);
proto flag; `call()` ceiling check; VA_PACK prologue op (array local);
spread CALL_SPREAD if in-scope. e2e: sum_all shapes, empty varargs,
varargs + defaults, spread of empty/spread counts, key-type errors N/A,
varargs capture (the packed array is a normal local). bench: varargs call
overhead vs lua's `...` (lua packs a table too — honest comparison) and vs
python *args.

**bench additions** (the suite): `mr_call` (two-result call + destructure in
a loop, vs lua multi-return and python tuple-return), `varargs_pack`
(sum_all shape, vs lua ... and python *args). lua's multi-return is the
interesting one: lua returns stack slots, we return register slots — both
zero-copy in spirit; python pays a tuple allocation. we should WIN that
bench; if we don't, the window-at-register binding didn't land and the
patch isn't done.

## 8. rulings — OPEN (none of these block drafting patch A except R1/R2)

- R1 destructuring shape: `let (x, y) = f();` with per-binder mut —
  Z's recommendation, parens because the comma-list-under-let is spent
  (for heads) and because the span is a thing, not a list of bindings.
- R2 result arity mechanism: INFERENCE (every return path agrees, checked
  at compile) vs ANNOTATION (`fu f() -> 2`). Z leans inference: parameter
  arity is already structural, no second syntax to maintain, and the
  mismatch error points at both sites. the annotation's one virtue —
  interface documentation — is what 0.5.0's type syntax is for.
- R3 varargs token: `...` (new lexeme) vs `*b` (existing token, C-deref
  reading). Z leans `...`.
- R4 `print` of a span call: error (expression position wants 1) — or
  should print get a special multi-arg form? Z says error v1; print takes
  one value, always has.
- R5 spread: ship with B or defer to the std seed. Z leans defer unless
  the token work is already in.
- R6 no adjust-to-receiver: a 2-result call destructured into 1 binding is
  a compile error, not lua's silent take-first. strictness doctrine; the
  lax path, if ever wanted, is a future `first_of(f())` builtin.
- R7 (naming): the milestone is "spans" in the roadmap; the SURFACE never
  says the word. confirm the name stays roadmap-internal (Z assumes yes).

## 9. what maps already bought us (reuse inventory)

- window-at-register locals: proven by the for-in hidden state
  (`" iter"/" cur"/" end` adjacent, asserted) — the destructuring let is
  the same trick user-visibly.
- `requested_count` threading in compile_expression: the MR hook.
- OP_RETURN range + OP_CALL retc: implemented, e2e-covered via dynamic
  retc (`c == 0`) tests.
- the compile-time "unimplemented multi-result" CRASH_MSGs are the exact
  TODO markers to delete.
- iteration precedent: `for let k, v : m` already delivers TWO values per
  trip through dedicated loop registers — spans formalizes what the ITER
  ops do ad hoc. (a future generic protocol, if ever, generalizes over
  both; still not needed.)
