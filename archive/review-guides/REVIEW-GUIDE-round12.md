# Review guide — round 12 (buffer night), patch 04

One commit on top of `55e1293` (master at time of writing): `9630c91`.
**24 files, +726/−101.** 200/200 e2e × gcc/aggressive/asan/asan-aggr in my sandbox.
Apply with `git apply 04-buffer-night.patch` on a clean `55e1293` checkout.

This round is *five* features sharing a few files, so this guide maps hunks →
features → what to look at, in the order I'd review them.

## 1. Ternary: `c then a otherwise b` (new feature)

Zero new opcodes — it compiles to the if-statement's shape folded into an
expression. Both arms compile into the caller's result register (your
invariant; it held).

| File | What |
|---|---|
| `src/oktoken.h` | TOKEN_THEN / TOKEN_OTHERWISE added (X-macro + enum) |
| `src/oklexer.c` | keyword trie: `'t'→'h'→'e'→"n"`, `'o'→'t'→"herwise"` |
| `src/okast.h/.c` | `ast_conditional_expression` (condition/then/otherwise) + quartet; the commented-out X-macro line is now live |
| `src/okparser.c` | `parse_conditional_expression` — then-arm at PREC_CONDITIONAL (stops at `otherwise`), else-arm at PREC_CONDITIONAL−1 (right-assoc, same trick as assignment). Missing `otherwise` = clean parse error |
| `src/okcompiler.c` | `compile_conditional_expression` — condition → own temp, `JUMP_IF` falsy → else, arms → `p_dest`, plain `JUMP` over the else |

**Two landmines I hit and fixed — worth re-checking:**
- `parse_expression` dispatches on `previous`, so each arm needs an `advance()`
  past its keyword before parsing (same reason `parse_assign_expression` advances first).
- Arms/condition that *resolve without emitting* (constants, identifiers,
  in-place compound assigns) must be **`materialize_into`** (forcing) —
  `materialize_into_if_not_register` skips *any* register location and the
  result register would be stale garbage. This bit twice; both uses in the
  ternary now force.

Precedence: `PREC_CONDITIONAL` sits between ASSIGNMENT and OR (the enum slot
existed already). Assignments inside arms need parens — same strictness as C's
ternary middle. `x = c then a otherwise b` parses as `x = (c then a otherwise b)`.

## 2. Strings: `+` concat, and an equality bug fix

- `perform_add` object-lhs arm: string+string → concat, everything else still
  `invalid operands` (**no coercion** — `"a" + 1` errors; host-side `to_str`
  stays the formatting path). GC-safety argument is in the comment: combined
  bytes go over a *raw scratch buffer* (invisible to the GC), then
  `create_object_string` copies it; there is no allocation between
  store-append and register publication, so the array-NEW death window doesn't
  exist here. No guard needed — the comment walks through why.
- **Bug fix in `perform_equal`/`perform_not_equal`**: `strncmp(a, b, len_lhs)`
  made `"ab" == "abc"` **true** (prefix match). Now length-equal + `memcmp`.
  This was live on v0.0.5-master; the game would have hit it on any string
  compare with a prefix.

## 3. Lexer cleanup (your ask: dead cpp-era tokens)

Removed: `++`, `--` (rejected by decision), `->` (old lambda syntax), `ok`,
`operator`, `static`, `async` (stackless-coro / class-modifier era). Also
removed from `okoperator.h` and the declaration-modifier enum. `try/catch/throw/
finalize` **kept** (D7, planned) but now parse to a clean error instead of
`assert(0)` + release-build fallthrough to empty-statement.

Kept deliberately: the bitwise family (`^ & | ~ << >>` + compounds) — D3 says
JS-i32 bitwise is planned, and `and`/`or` reuse `&&`/`||` lexing. `?` is the
condition terminator, not a ternary — untouched. Removed keywords are now legal
identifiers (`let ok = 5;` works).

## 4. Subscript container elision (the parked MOVE-staging item)

`resolve_lvalue` gains a `p_pending_rhs` param (NULL for pure reads). When the
container compiled to a plain local's register (no code emitted) **and** the
index and pending rhs are *inert*, the lvalue references the local's register
directly and the staging temp is rolled back. `expression_is_inert` is the
conservative walker: calls and assignments fail it; lambda literals pass
(creating a closure runs no body); subscripts/binops/unary/array-literals
propagate. Rationale for not trusting `scope.captures_any`: it's set when the
lambda is *compiled*, which can be later in the function than the subscript
using the local — a forward-reference hole.

Effect (disassembly on your bench files):
`a[0] = a[0] + 1`: 5 dispatches → **3**; `a[0] += 1`: 4 → **3**; both forms now
identical modulo the extra read. Register-index and read-path (`a[i]` as rvalue)
elide too.

**Sandbox timing caveat:** my box is noisy — an identical-bytecode control
(global-container bench, unchanged codegen) swung ±8-9% between binaries, so I
do not claim ms numbers. The dispatch-count reduction is the evidence; your
hyperfine rig is the arbiter. Note my `build_ok.sh` has no `-O` flag (sandbox
habit from debug rounds) — use your release flags.

## 5. Call-RHS window fix (pre-existing bug, found by the gate tests)

`arr[0] = reseat()` — where `reseat` captures and reseats `arr` — **crashed on
master** ("expected an array"), and your `ok-reg-fresh` binary reproduces it.
Root cause: the assign compiled its rhs into `p_dest`, which sits *below* the
lvalue's staged base/index temps; a call rhs opens the callee's frame at
`[dest, dest + fu->registers)` and `call()` null-fills everything above the
args — erasing the staging. Compound assigns never had this (their `cur`/rhs
temps allocate after the staging). Fix: subscript-assign rhs compiles into a
fresh temp above the staging. `arrays/regressions/assign_call_rhs_window.ok`
pins it, including the wide-callee and nested-assign shapes.

## Tests (15 new files, the other half of your ask)

- `operators/ternary/`: basic, nesting (right-assoc chains), side_effects
  (only the taken arm runs; cond-once; arms-in-loops), arms (paren'd assigns,
  compound arms, call args, subscript indices, closures as arms), terminator
  (`;` vs condition-`?` positions), errors/missing_otherwise
- `operators/logical.ok` — **and/or had zero coverage** despite being
  short-circuit value ops. Pins: value semantics (`0 and 2` → 2), short-circuit
  (rhs never runs), the `x != null and x > 0` guard, precedence vs `not`
- `control/truthiness.ok` — pins the doctrine in one place: falsy is exactly
  `false`/`null`; 0, "", [] are truthy; through if/while/and/or
- `strings/`: concat (chained, `+=`, array elements, runtime-built == literal),
  equality (the prefix bug, mixed-type identity), errors (string+num both ways)
- `arrays/regressions/`: `assign_call_rhs_window` (the bug above),
  `container_elision_gates` (reseat-preservation for rhs-assign, closure-call,
  index-call; elided hot paths), `eval_order` (base→index→rhs, observable)

**Known gaps found, not fixed tonight (your call):**
- `%` parses (pratt row, OPERATOR_BIN_MODULO) but has no opcode — `i % 2`
  compile-aborts with "unimplemented binary value operator". Pre-existing.
  One opcode + two VM cases when you want it; my side_effects test works around
  it and says so in a comment.
- Unit tests need Unity (CMake) — not runnable in my sandbox; this round
  touches none of their units (no new opcodes, chunk/table/nan/array untouched),
  so your normal CMake run covers them.
- The e2e runner can't pin empty-line outputs (`// expect:` with nothing
  doesn't survive the grep) — worked around with `== ""` checks; worth a runner
  tweak someday.

## Suggested apply order / verification

1. `git apply 04-buffer-night.patch` on `55e1293`
2. Full e2e + your error-path checks (expect 200/200)
3. `ok --dump` any of: bench/array_subscript_add_plain.ok (3 dispatches),
   a ternary one-liner (JUMP_IF + JUMP shape, arms share the dest register)
4. `./ok` on: `print("ab" == "abc");` → false; `print("a" + "b" + "!");` → ab!
5. If you want the perf read: hyperfine the subscript benches old-vs-new on
   your rig; plain form should drop visibly, compound slightly, globals flat
   (their bytecode is unchanged — any delta there is machine noise)
