# oklang 0.5.0 (Citadel) — classes round 2, the operator surface

status: patch 32, the first of the three closing rounds, written against
your classes-staging master (patches 30 + 31 applied, the state R1 gated
green on). this is the record of what your rulings turned into this
round: R1 accepted and implemented (your ruling, no veto), the full
operator surface, the mangled-name answer you asked about, two real
bugs the round found in the shipped R1 code, and the copy-on-redefine
analysis you asked for at the end (§5). §6 is what remains.

## §0 — what your message ruled, and what happened

your R1–R8 pass landed as ruled; only R1 changed code, and it was a
delight to implement — the honest answer to your "is there an impl
constraint i don't see?" is: there was one, and the freeze pass
dissolves it. details in §1. everything else (R2–R8) confirmed the
shipped behavior; nothing moved.

the round's own new sub-rulings (OPEN, your vetoes, §4):
**R9** the shadow order, **R10** the unary/binary minus split,
**R11** the class-resident operator cache, **R12** the `operator`
reservation. each is a contained revert if you hate it.

## §1 — R1: methods in bare-name resolution, the shape it took

the objection in R1 was real: method tables are not name-resolution
environments, and making the compiler pretend otherwise means inventing
a compile-time lie. but the maybe-lane already exists — the field lane
emits `GET_FIELD_NAME` (a name, resolved at freeze) for every
unresolved bare name inside a member body. the method ladder rides the
SAME op. the freeze pass, which already knows the complete tables,
now bakes:

- a field of this chain → the slot op (as before)
- else a method of this chain → **`OP_GET_METHOD`**: bind `this`
  (register 0) to the entry found on the RUNNING method's defining
  class — the same entry every time, because the tables are sealed
- else a global → `LOAD_GLOBAL` (as before)
- a STORE to a method name → the not-assignable trap

```javascript
class Animal {
  let name = "";
  fu init(name) { this.name = name; }
  pub fu describe() { return name + " speaks"; }
  pub fu speak()   { return describe() + " loudly"; }  // bare sibling
  fu secret()      { return "hidden"; }
  pub fu reveal()  { return secret(); }                // bare private
  pub fu both()    { return describe() + " | " + this.describe(); }
}
// a.speak() == "rex speaks loudly"; a.reveal() == "hidden";
// a.both() == "rex speaks | rex speaks"
```

exactly your ruling: implicit `this` binding for the common case,
explicit `this.method()` keeping the fused INVOKE fast lane. statics
got the mirror at runtime (they have no freeze bake — statics resolve
by name at runtime already): `GET/STORE_STATIC_NAME`'s miss path now
tries static methods (a sibling call binds `this` = the class value)
and then globals. that miss path fixed a real bug (§3).

the one shape to know: reads are field-first, call sites are
method-first (§4/R9).

## §2 — the operator surface

every perform ladder gained the instance arm, after the number and
string lanes, before the error: **`+ - * / % == != < <= > >=`, unary
`-`, `[]`, `[]=`, and the instance `operator ()`** (through OP_CALL —
the instance already sits at the call window's receiver position, so
it costs nothing to stage). C14 exactly: dispatch on the left operand
only. `!=` is `==` plus a truthiness negation — no separate operator.
the ordered four return the method's raw value; truthiness happens at
the consumer, same as `==`.

```javascript
class Vec2 {
  let x = 0; let y = 0;
  fu init(x, y) { this.x = x; this.y = y; }
  pub fu operator +(other) { return Vec2(x + other.x, y + other.y); }
  pub fu operator ==(other) { return x == other.x and y == other.y; }
  pub fu operator [](i) { return i; }              // subscripts too
}
let a = Vec2(1, 2);
let b = Vec2(3, 4);
print((a + b).x);        // 4
print(a == Vec2(1, 2));  // true
print(a.operator +(b).x); // 4   the member syntax, same entry
let m = a.operator +;    // and it binds like any method
print(m(b).x);           // 4
```

**the mangled-name question you asked** ("it should be human readable,
and accessible via class, so they can be bound as well dont you
think?"): yes, and that is what shipped. the canonical name is
literally `"operator +"` — keyword, one space, spelling — synthesized
by ONE shared parser helper used by both the declaration site and the
member-access site, so `fu operator+(o)` (no space) and
`a.operator  +(b)` (extra spaces) produce the identical string. human
readable in every diagnostic, one entry in the method table, reachable
and bindable through the ordinary member syntax. no second access
mechanism needed; the answer to "or provide another way?" is "the
member way IS the way".

**the cache**: your plan sketch said "operators + MIC (simple cuz
frozen)". frozen is exactly right, and it buys something better than
per-site MIC: the encoding has no spare bits on 1-word ops (you'd know
— you packed them), so instead the cache lives ON THE CLASS —
`op_cache[CLASS_OP_COUNT]`, one entry pointer per (class, operator),
filled on first dispatch. sealed tables mean find_method is a pure
function of (class, name): the entry can never dangle while the class
is reachable, and one cache serves every dispatch site in the program.
zero instruction words spent. (§4/R11 for the veto.)

the cold path cost is honest: dispatch_operator_method stages the
receiver and args above every live register and runs a nested
run_until — the construction/defer machinery, already proven. a
binop's (dest, lhs, rhs) shape carries no call-window guarantee, so
the frame's own registers are never touched. the dispatch cases
refresh their cached exe/reg/frame pointers after it (the nested call
can realloc both arrays — the same dance the construct path does).

## §3 — the bugs the round found in shipped R1 code

both found by probing before writing code, both fixed and pinned:

1. **a class declared inside a method hard-errored at definition
   time.** the enclosing class's freeze walk descends into every
   function-valued constant of every member body — and a class
   declared in a method compiles ITS members as constants of that
   method's chunk. so `Outer`'s freeze patched `Inner`'s protos for
   `Outer`'s layout, and the very first `Inner` definition then died
   with "already defined" — or worse, a wrong-slot bake. fix: protos
   carry `is_class_member` (set at begin_context for METHOD/STATIC);
   the freeze walk stops at member boundaries. pinned by
   `class_in_method.ok`.
2. **bare statics had no fallback.** `GET/STORE_STATIC_NAME`'s miss
   path went straight to the error, so a bare sibling static call
   (`b()` from static `a()`) and a bare `glob` reference from any
   static context were both unreachable. the miss path now runs the
   ladder: static field → static method → global. pinned by
   `bare_static_method.ok`.

and one the gates caught in THIS round's code: asan flagged a
read-after-free of the error token in the `operator`-field reservation
(two lines, captured the token before the free). the receipts
tradition continues.

## §4 — R9–R12, open for your veto

- **R9 the shadow order**: a class may declare a field and a method
  with the same name. reads resolve field-first (data wins):
  `obj.x`, `this.x`, bare `x` all read the field. call sites fuse to
  INVOKE and resolve method-first (invoking wins): `obj.x()` and
  `this.x()` call the method. a bare name in call position (`x()`) is
  a read that gets called — the field's value. the alternative orders
  break more than they fix; this one is coherent and pinned in
  `bare_method_call.ok`.
- **R10 the unary/binary minus split**: `operator -` is one name.
  one-name-one-owner (R4's doctrine, now enforced within a body too —
  a same-name redefinition is refused instead of silently replacing)
  plus the signature contract means a class picks either the unary or
  the binary form, and a hierarchy cannot flip the arity. if you want
  both in one hierarchy, that is a table-shape change (name+arity
  keys) — deferred unless you rule otherwise.
- **R11 the class-resident cache** (§2): supersedes the plan's
  "per-site MIC". one slot per (class, operator) shared by all sites;
  the per-site shape returns the day an operator goes polymorphic at
  one site (which sealed tables make impossible today).
- **R12 the `operator` reservation**: a FIELD named `operator` is
  refused at parse (`.operator` always fuses into an operator member
  name; the field would be unreachable). methods named `operator`
  remain exactly what they were — operator declarations.

## §5 — the copy-on-redefine question (your `make_class` example)

you asked: how cheap would a VM copy be, to allow

```javascript
fu make_class() {
  class Inner { let t = 1; pub fu get() { return t; } }
  return Inner;
}
let C1 = make_class();
let C2 = make_class();   // today: "a local class can only be defined once"
```

the honest accounting. the class OBJECT is already per-call (OP_CLASS
runs fresh each time) and the closures are already per-call (OP_CLOSURE
captures fresh upvalues). the ONLY shared thing is the proto — the
compiled body — and the freeze pass rewrites its chunk code IN PLACE
(slot bakes, traps, GET_METHOD bakes). C2's freeze cannot rebake what
C1's freeze already rewrote.

so the copy is: on a re-freeze of a proto (patched_by set and not
mine), deep-copy the proto — new object_function, copied code array,
copied constants array (values shared, not duplicated), function-valued
constants remapped through an old→new proto map so nested lambdas
follow — then swap the closure's function pointer and freeze the copy
for the new class. ~120-160 lines, one contained function plus a
branch in freeze_patch_proto, no new opcodes, no surface change, and
the error test flips to a positive test. the soundness argument is
easy (same bytecode, own bake; MICs and binds point at frozen tables
that stay valid). the cost is memory per redefinition and one more
subtle invariant to keep honest under aggressive GC.

my recommendation: worth doing, as its own small patch AFTER 33/34
(33 touches the same freeze pass for `operator Target` conversions —
landing the copy first would mean rebasing it). it closes the last
shape where the in-place freeze says no, and "class factories work"
is a real language property. say the word and it is patch 35.

## §6 — what remains of 0.5.0

- **patch 33 — conversions + identity**: the `===` token (primitive
  identity, total); `is` (C12's yes — classof+identity fused, no
  global involvement); `isinstance`/`classof` builtins; the `as` lane
  through `operator Target` (this is where `TAG_AS`, `TT_`, and the
  `" str"` hidden globals die, per your addendum); the builtin class
  singletons (num/bool/str/array/map/function/nil) under Object, and
  the nil/null binding per C10.
- **patch 34 — natives as classes**: `ok_native_fn` entries in class
  tables (receiver staged as argv[0]), the sealed str/array/map
  surfaces, the embed section. the toughest, as you called.
- **patch 35 (proposed)** — the copy-on-redefine from §5.

and your re-schedule is logged: after classes close, a small
optimization round before the next theme. the classes benchmarks you
floated (delta blue; wren added to the matrix) belong THERE, next to
the profiling — this round was not small (16 files, 15 tests, two
shipped-bug fixes), so per your own conditional they did not make the
cut. wren in the matrix is a good call; its bench harness shape
(hyperfine + the existing run-benchmarks.sh) means one more column,
not a new pipeline.

## §7 — applying

`git apply patches/32-operator-dispatch.patch` from the oklang root
(on top of 30+31, in order). verify: `build_ok.sh plain` + the e2e
runner; the new suite is `tests/e2e/tests/classes/operators_*.ok` +
`bare_*.ok` + `class_in_method.ok` + the three new error tests. the
gate line to match is **379 x4 + unit x3 + embed x3**. formatting:
hand-matched as always, run your clang-format for the canonical pass.

— z, session 28. Citadel: two rings up, the wall continues.
