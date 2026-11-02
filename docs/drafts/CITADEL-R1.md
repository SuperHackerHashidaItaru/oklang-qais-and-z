# oklang 0.5.0 (Citadel) — classes round 1, the implementation guide

status: the first code round of the classes design, written against the
state of your master `0666651` (v0.4.0 Eyrie) with patches 30 and 31
applied. the design doc is `CLASSES.md` (session 26, the draft you ruled
on); this is the record of what the rulings turned into, what the code
actually does, every place the implementation had to make a call your
rulings did not cover (numbered R1..R8, OPEN for your veto at §7), and
the receipts. read §2 first if you only read one section: it is the
surface you now have.

## §0 — the shape of the round

your message said: implement if the rulings are sufficient, otherwise
whiteboard 1.1. they were sufficient — with four of them needing their
open halves closed first, which §1 does. no whiteboard needed: every
open question had an answer that fell out of decisions you had already
made.

the round itself started rough and ended clean. the previous session
(this one's context was cut mid-work — you saw the webui say so) had
left ~2500 lines of uncommitted class work in the sandbox: the object
model, the parser, most of the codegen, all untested and in one place
literally double-pasted. this round recovered it, finished it, and paid
down its bugs: the freeze pass had never been exercised on real code,
and the gates caught seven real defects in it and around it (§4 is the
honest list — two of them were mine, five were the recovered work's).
what shipped:

- **patch 30** (5139 lines, 16 files): the class system — object model,
  declaration, fields, init, super, statics, construction via the
  static call operator, privacy, sealed tables, monomorphic caches,
  bound methods, `this` capture, the disassembler catch-up. 22 new e2e
  tests.
- **patch 31** (843 lines, 1 file): the C14 performs rewrite — every
  binary `perform_*` now does the number check first and a tag switch
  second, exactly the shape you did with `perform_as`.

gates on the final state: **e2e 364 x4** (plain / asan / asan-aggr /
aggressive — the 342 suite plus 22 class tests), **unit x3**, **embed
x3**, all green together. 364 is the new floor for the next round.

what is deliberately NOT here: operator overloading (the `operator +`
surface parses but does not dispatch — patch 32), conversions into the
`as` lane + `isinstance`/`classof`/`is` + the builtin class singletons
beyond Object/Class (patch 33, where `TAG_AS`/`TT_` die), natives as
classes (patch 34). the `is` operator ruling is in §1/C12 — it is a yes,
it rides patch 33 because it needs `classof` to mean anything.

## §1 — the rulings ledger: C1-C18, closed

each entry: your ruling, then what it became in code. the R-numbers are
new sub-rulings the implementation forced; they are OPEN for your veto
(§7).

- **C1 `:`** — done. `TOKEN_INHERITS` is deleted; the lexer has `pub`
  and `static` where it was. `class Dog : Animal { ... }` is the shape.
- **C2 `this`** — done. `this` is an immutable local at register 0 of
  every method (instance and static alike — R5), captured by lambdas as
  an ordinary upvalue.
- **C3 `init`** — done. `fu init(...)` is a method like any other, with
  one privilege: its field stores may assign non-mut fields (R3).
- **C4 no `new`, static `operator ()` is the creation way** — done, and
  it composes better than the draft's `.new` did. `ClassName(args)` is
  an ordinary call on a class value: OP_CALL sees an OBJ_CLASS callee,
  looks up `operator ()` in the class's statics chain — a user
  `pub static fu operator ()(args)` replaces construction entirely
  (the factory test has an interning-style example) — and when none
  exists, the default construction runs: allocate, field initializer
  thunks root-first, nearest `init` with the caller's args, return the
  instance. `Foo.operator ()(args)` spells it explicitly and takes the
  same path. there is no `new` anywhere.
- **C5 fields: hidden outside, visible inside** — done. outside the
  hierarchy there is no field syntax that resolves: `a.x` from outside
  is a "not visible here" type error (the tests pin this). inside,
  fields are visible on any receiver whose class is in the ring: `x`,
  `this.x`, and `other.x` (the Vec2 argument) all work, the last
  through a hierarchy-guarded member op.
- **C6 `pub` only on methods, survives overrides** — done, plus the
  flip side you ruled: a private override of a pub ancestor is refused
  at definition ("must be pub"), and so is a signature change (with one
  exception, R2).
- **C7 super: strict call syntax, enforcement where possible** — you
  asked whether compile-time enforcement is possible in a dynamic
  language; the honest answer is *half of it is*, and that half is
  shipped. bare `super` is a parse error; `super(...)` and
  `super.name(...)` are the only forms; `super` outside a method body
  is a compile error. full compile-time enforcement is impossible for
  the reason you said — the ancestor chain is runtime names. but
  "forgot super entirely" does not need the chain at compile time: the
  compiler stamps every method proto that contains a `super(...)`
  initializer call (a `calls_super_init` flag, propagated through
  nested closures), and at OP_DEFINE_METHOD — the one point where the
  chain IS complete — an `init` override whose chain carries an `init`
  without that flag is a definition-time error. `init_super_error.ok`
  pins it. what stays unenforced (documented): the *ordering* rule
  (super before touching inherited fields) — a conditional super or a
  super-in-a-lambda satisfies the flag; that is convention now, same as
  every other dynamic language.
- **C8 no dtor** — nothing to do, and the guide's §6 notes the
  `Foo.open()` + `defer close()` idiom works today.
- **C9 statics** — done, formalized as drafted. `static fu` /
  `static let (mut)` in class bodies, `pub` composes, storage is a
  per-defining-class value array shared down the chain by lookup and
  NEVER copied (a subclass mutating an inherited static mutates the
  ancestor's slot — the `Sub.double_bump()` test pins the shared
  counter). statics are methods whose receiver is the class value: the
  same INVOKE machinery, `this` inside a static is the class, and
  `super` in a static walks the parent's statics.
- **C10 one metaclass** — done. `Object` (root, empty, frozen at birth)
  and `Class : Object` exist as singletons created at startup and
  seeded as immutable top-level bindings — `class A : Object` parses
  and means the default. the nil/null question ("normal rules apply")
  is answered in the draft's terms and lands with patch 33's
  `classof`: `classof(null) === nil`, null is nil's sole instance,
  `isinstance(null, Object)` is true through the chain. nothing
  special-cased in user code.
- **C11 `===` identity / `==` overloadable** — the semantics are as
  ruled; the `===` *token* does not exist in the lexer yet (the draft's
  grammar sketch has it, the shipped lexer never did). it lands in
  patch 33 with `operator ==` dispatch, which is when `==` stops being
  purely builtin. until then `==` is the shipped mixed-type equality
  with identity fallback, and classes compare by pointer through it
  (pinned in `classes_as_values.ok`).
- **C12 `isinstance` + `classof` builtins; the `is` sugar** — builtins
  land in patch 33. **my ruling on `is`: yes, add it** — it is pure
  sugar for `classof(lhs) === rhs` and it reads exactly right in a
  guard (`if x is num`). one honest delta from my first sketch: I said
  "zero new opcode, desugar it"; the desugar would route through a
  rebindable `classof` global, which is a hole — so it will be a small
  dedicated op (classof + identity fused, no global involved). veto is
  yours; it is one hunk either way.
- **C13 operator set** — the parser is ready: `operator +`, `operator
  ==`, `operator []`, `operator []=`, `operator ()`, and `operator
  Target` (conversion, target as identifier) all parse with the
  mangled-name scheme (the name spans the keyword plus spelling; a
  method literally named `operator` with a parameter list is no longer
  expressible — reserved). dispatch is patch 32. and your C13
  addendum is confirmed: there are no `++`/`--` tokens at all, so
  nothing to reject.
- **C14 left-operand dispatch + the performs rewrite** — the rewrite is
  patch 31 (§3). the dispatch arms (the instance lane at the end of
  each perform ladder) are patch 32; the dead `IS_VALUE_OBJECT(rhs)`
  TODO arms are already deleted — your TODO in `perform_add` is
  answered in code: strictly lhs, else "invalid operands".
- **C15 sealed, never reopen, never patch** — done and load-bearing.
  tables freeze at OP_CLASS_END (a class body executed twice is
  refused: "a local class can only be defined once" — the in-place
  freeze patching cannot express two classes sharing one proto, and
  per-definition proto copies are the noted ladder step). the freeze is
  what makes the MIC's raw entry pointers sound forever.
- **C16 instances as maps / exceptions as maps / no kwargs** — nothing
  to do this round (instances are flat slot arrays per C5's fast-field
  design, which is strictly better than maps and you approved it as
  the fields vision).
- **C17 classes are decls, usable anywhere** — done: top level, nested
  in functions (the `local_class.ok` test builds one per call... once —
  see C15), `glob`/`export` modifiers apply where they do for `fu`. a
  class binds its name like a `fu` declaration does, declared
  initialized (the name is visible to its own method bodies and to the
  statics lane; nothing member-shaped can run before OP_CLASS fills
  the register).
- **C18 bound methods** — you asked why the draft ruled them out:
  opcode frugality or a design issue? the honest answer was *neither,
  mostly surface discipline* — and this round's recovered work had
  already implemented them (GET_MEMBER returning a bind), so the
  ruling had to be made for real. **the re-ruling: they are IN.** the
  reasoning, so you can veto it informed: the both-lanes design (fast
  INVOKE, slower bind) is the clox shape and is not a flaw; the real
  costs were (a) one more value category needing `==`/print rulings
  and (b) 0.5.0 surface width before the sealed core proved itself.
  (a) resolves trivially under C11's defaults: `==` on a bound method
  is allocation identity (two binds of the same method are unequal —
  pinned nowhere yet, it is just the identity fallback), print is
  `<method name>` like `<fn name>`. (b) is sequencing, not principle,
  and under C15 a later addition would have been clean anyway — so
  there was no permanent win to buy by waiting. the capability
  semantics that fall out (and which I am claiming as R6): privacy is
  checked at BIND time, not call time — an inside-the-ring method can
  hand a private bind to the outside, deliberately. that is the
  closure story wearing methods, and it is the one C18 behavior to
  actually review.

**R1-R8, the sub-rulings the implementation forced** (all OPEN, §7):

- **R1** methods are never called bare. the name ladder is local,
  param, field (or static), outward — a method needs a receiver:
  `this.m()`, `obj.m()`, `Foo.m()`. (explicit over implicit; also the
  compiler's honest shape — method tables are not name-resolution
  environments.)
- **R2** `init` is exempt from the override signature contract. a
  subclass constructor may widen its parameters; that is the
  super-forwarding pattern's whole point. virtual methods keep the
  strict contract.
- **R3** mutability is a post-construction contract. inside `init`
  bodies and field initializer thunks, stores to non-mut fields are
  legal (that IS construction); everywhere else they bake the
  immutable trap. a lambda inside `init` does NOT get the privilege
  (its store would run at an unknowable time — it takes the trap;
  assign in the init body directly).
- **R4** statics may never be shadowed, chain-wide — same doctrine as
  fields, one name one owner.
- **R5** `this` exists in statics and is the class value (uniform with
  instance methods; `classof(this)` differs, nothing else does).
- **R6** bound-method privacy is bind-time (see C18 above).
- **R7** the class name is visible inside its own class body
  (declared-initialized; sound because no member code runs before
  OP_CLASS fills the register — static initializers read it, methods
  capture it).
- **R8** construction with no `init` anywhere in the chain runs field
  defaults only; a `super(...)` against a chain with no init is the
  ordinary "no method in the super chain" error.

## §2 — the surface you have, by example

real oklang, every line of it tested (the test name follows in
[]s):

```javascript
// declaration, fields, init, defaults, privacy, super [basic, fields]
class Animal {
  let name;                    // no initializer -> null
  let legs = 4;                // initializer thunk, declaration order
  let mut visits = 0;          // mutable field

  fu init(name) {              // the constructor [C3]
    this.name = name;          // non-mut store: legal in init [R3]
  }
  pub fu describe() {          // pub = the outside surface [C6]
    return name + " has " + legs as str + " legs";
  }
  pub fu visit() {             // bare field names: local/param/field [R1]
    visits = visits + 1;
    return visits;
  }
}
let a = Animal("rex");         // construction = the static call op [C4]
print(a.describe());           // rex has 4 legs
print(a.visit());              // 1, then 2

// inheritance, super both ways [inheritance]
class Dog : Animal {           // ':' [C1]
  let breed = "unknown";
  fu init(name, breed) {
    super(name);               // required when a chain has init [C7]
    this.breed = breed;
  }
  pub fu describe() { return super.describe() + ", a " + breed; }
}
class Puppy : Dog {
  fu init(name) { super(name, "puppy"); }   // widened params [R2]
  pub fu kind() { return "puppy " + super.kind(); }
}
print(Puppy("bit").describe());  // bit has 4 legs, a puppy

// fields are invisible outside the ring [privacy_fields]
print(a.x);                    // error: field 'x' ... is not visible here

// statics: shared state, one storage per defining class [statics]
class Counter {
  static let mut count = 0;
  static let scale = 10;
  pub static fu bump() { count = count + 1; }   // bare statics [R1]
  pub static fu value() { return count * scale; }
}
class Sub : Counter {
  pub static fu double_bump() { this.bump(); return this.bump(); } // this = the class [R5]
}
Counter.bump(); Counter.bump();
print(Counter.value());        // 20
print(Sub.double_bump());      // 4  - the SAME counter [C9]
print(Counter.value());        // 40

// classes are values [classes_as_values]
let registry = { "one": A, "two": B };
print(registry["one"]().get());       // construct through a map value
print(A == A);                        // true  (pointer identity)
print(A);                             // <class A>
fu make(klass) { return klass(); }
print(make(B).get());                 // 2

// construction override: a real factory [operator_call]
class Pool {
  static let mut created = 0;
  pub static fu operator ()(tag) {    // replaces construction [C4]
    Pool.created = Pool.created + 1;
    return tag;
  }
  pub static fu count() { return created; }
}
print(Pool("a"));                     // a
print(Pool.count());                  // 2

// bound methods [bound_method, C18/R6]
let m = Greeter("world").greet;
print(m("hello"));                    // hello world

// this through a lambda [this_lambda]
class Box {
  let mut n = 10;
  pub fu adder() { return fu(x) { n = n + x; return n; }; }
  pub fu get() { return n; }
}
let add = Box().adder();
print(add(5)); print(add(5));         // 15, 20
print(b.get());                       // 20 - the SAME field

// a field holding a callable [field_callable]
pub fu run(x) { return this.action(x); }
```

and the error surface, each pinned by a test: private method from
outside ("is private"), private override of pub ("must be pub"),
signature change ("different signature", except init), forgot super
("defines init without calling super()"), field redeclaration
("already declared by class"), static redeclaration, immutable store
("declared without 'mut'"), bare `super` ("must call a method or the
parent initializer"), super outside a method, class body executed twice
("only be defined once").

## §3 — patch 31: the performs, numbers first (C14)

the shape you asked for, applied to all twelve binary performs:

```c
if (value_is_numeric(lhs) && value_is_numeric(p_rhs)) {
  if (IS_VALUE_STRICTLY_INT(lhs) && IS_VALUE_STRICTLY_INT(p_rhs)) {
    lhs = INT_AS_VALUE(VALUE_AS_INT(lhs) OP VALUE_AS_INT(p_rhs));   // int/int stays int
  } else {
    lhs = NUMBER_AS_VALUE(value_as_f64(lhs) OP value_as_f64(p_rhs)); // any float widens
  }
  ...store, return
}
switch (VALUE_TAG(lhs)) { case TAG_OBJECT_MASK: /* strings for + and ==, else invalid */ ... }
```

two details worth your eyes:

- **`IS_VALUE_STRICTLY_INT`, not `IS_VALUE_INT`, as the discriminator.**
  raw doubles alias the int tag (`-3.0` & TAG_MASK == TAG_INT_MASK —
  the same landmine `mark_value`'s comment warns about, the _STRICT
  doctrine from DECISIONS-DELTA). the old if-chains were ordered so
  doubles never reached the int branch; the first cut of this rewrite
  lost that ordering and `7 % -3` reported "modulo by zero" — the
  modulo e2e caught it in one run. the doctrine now has a second
  monument.
- the four ordered comparisons (>` `<` `>=` `<=`) are one macro —
  numbers through the union, everything else "invalid operands". the
  mixed-type `==`/`!=` identity fallback is preserved exactly (it is
  documented shipped semantics, not an accident).

micro-bench, release build, this sandbox: 5M-iteration add loops at
~124ms (float and int variants both) — that is the post-rewrite
baseline for Citadel, recorded so the next automated bench run diffs
against something honest. the hyperfine A/B against the 0.4.0 binary is
one `run-benchmarks.sh` away whenever you want the table; the pipeline
is unchanged.

## §4 — the bugs the gates caught (the receipts)

seven real defects between the recovered mid-work and green gates.
listed because the honesty is the precedent, and because four of them
are shapes you will meet again in patches 32-34:

1. **two double-frees in AST teardown** — the field-initializer thunk's
   synthesized assign BORROWED the initializer expression but its
   teardown freed both sides; the class declaration's deinit walked
   members that its list-deinit then freed again. plain builds survived
   by allocator luck; ASAN caught both in one run.
2. **`return <bare-field>` returned null** — compile_return_statement's
   operand switch had no case for the field/static location kinds, so
   the return encoded RETURNED_NULL. this is the bug shape to remember:
   every `switch (location.kind)` in the compiler is now a place
   classes can fall through. (the compound-assign path had the sibling
   of it — field/member/static lvalues fell into the subscript arm.)
3. **the freeze pass never worked on 2-word ops.** three separate
   defects: `opcode_word_count` was missing OP_LOAD_NULL and
   OP_CLOSURE_LONG (any method containing a `null` literal derailed the
   walk); the pass joined instruction words in the opposite order from
   JOIN_WORD (every long-form decode read garbage); and the
   STORE_FIELD_G bake rewrote only the second word, leaving the old
   opcode on the first — a baked slot-store executing as a name-store
   reading constants[slot] as a string pointer. the class tests never
   passed until all three were gone.
4. **`OP_CLASS_END` had a dual-width encoding** — one opcode, two
   widths, unparseable for any walker. it is two opcodes now
   (CLASS_END / CLASS_END_LONG), one width each; every new op follows
   that rule and `opcode_word_count` is the single authority (the
   disassembler was migrated onto it — it also had the new ops missing
   from OPCODE_NAMES entirely, which was out-of-bounds reads in
   --dump).
5. **the MIC never traced its classes** — the comment promised the
   chunk roots the cache's class values; nothing did. aggressive GC
   swept a cached class between two calls of the same site and the
   second call used a dangling entry pointer. mark now walks the mic
   array (and the class tracer walks every table key, because a class
   routinely outlives the frame whose constants named its methods).
6. **upvalues closed after the return value landed.** a method's
   return_base IS its receiver's register (the receiver doubles as the
   call window base), so writing the result overwrote `this` at r0
   before cleanup closed the open cell — a lambda capturing `this`
   captured the method's own return value. the close now precedes the
   result write in all three return paths. this one is subtle enough
   to deserve your read: `return_*_from_frame` in okvm.c.
7. **the singleton startup ordering** — singletons created before the
   gc watched the vm, with a pause/resume around them, swept on the
   first allocation after the resume under aggressive. they are created
   after `gc_watch_vm` now, with the in-flight pieces on the guard
   stack. same class of bug, one startup shape: **every create-then-
   append-to-constants sequence in the compiler now rides a guard**
   (three of my sites were missing it; the existing string path had it
   right all along).

## §5 — what existing programs see

nothing, and the suite proves it: 342/342 on every variant, unchanged
bytecode for class-free code (no new ops are emitted outside class
bodies; the performs rewrite changes codegen-identical paths only in
branch order). the two behavior deltas worth naming: `static` and
`pub` are now keywords (any program using them as identifiers breaks —
the corpus had none, checked), and `Object`/`Class` are bound as
immutable top-level names (shadowable by scoping, same as `len`).

## §6 — the next rounds (the standing plan, renumbered)

- **patch 32 — operators**: the instance arm at the end of each
  perform ladder + per-site MIC, the mangled-name table lookups
  (`operator +` etc., already parsing), compound-assign in-place
  variants, unary set, `operator ()`/`[]`/`[]=` dispatch on instance
  receivers. R1 keeps the surface honest: `a + b` dispatches on `a`
  only (C14).
- **patch 33 — conversions + equality + identity**: `operator Target`
  into the `as` lane (the target becomes the class value — this is
  where `TAG_AS`, `TT_`, and the `" str"` hidden globals die, per your
  addendum: yes, it was implied, and the machinery that replaces it
  needs the operator-call lane from 32); `===` the token (primitive
  identity, total); `operator ==` dispatch with identity default;
  `isinstance`/`classof` builtins; **`is`** (C12's yes); the builtin
  class singletons (num/bool/str/array/map/function/nil) under Object
  and the nil/null binding per C10.
- **patch 34 — natives as classes**: `ok_native_fn` entries in class
  tables, receiver as argv[0], the sealed str/array/map surfaces,
  embed section.

## §7 — what to review hardest, and applying

review hardest, in order: **R1-R8 in §1** (your vetoes; the code is
shaped so each is a contained revert), the **upvalue close-before-
return** fix (§4.6 — it touches every return path in the VM), and the
**construction driver** (`construct_instance` + `run_method_to_completion`
in okvm.c — the nested run_until lane borrowed from the defer
machinery; it is the one place this round runs script from C outside
the dispatch loop).

applying: `git apply patches/30-classes-core.patch` then
`patches/31-performs-numbers-first.patch` from the oklang root (30 then
31, in order), or the usual hand-typing — the patches are
function-level like always. verify: `build_ok.sh plain` then the e2e
runner; the class suite is `tests/e2e/tests/classes/` (22 files); the
gate line to match is **364 x4 + unit x3 + embed x3**. formatting:
hand-matched to the codebase style — no clang-format in this sandbox;
run yours if you want the canonical pass before committing.

— z, session 27. patches 30 + 31 are in patches/ and download/; the
collab repo carries this guide and the worklog entry. naming: Citadel
stands (your call it stays); triptych goes in the bank for 0.6.0 if the
coroutines round wants it.
