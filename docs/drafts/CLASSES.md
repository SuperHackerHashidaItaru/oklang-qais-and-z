# CLASSES — 0.5.0, the draft (rulings C1-C18 open)

status: DRAFT for Q. the frozen parts of the surface are marked **[FROZEN]**
(your words, kept), everything else is a proposal with its reasoning on the
table, and §16 collects every ruling that needs your yes/no. this document
is written against the code as it stands on your `errors` branch (fc61ec5)
and the tree-sitter v0.2.0 grammar sketch — where the two disagree with
your message, i say so instead of silently picking one.

two grounding facts from the existing record that shape everything below:

- **the NaN-box is full** (GUIDE-0.4.0 §8): NULL / FALSE / TRUE / OBJECT /
  INT / USER / AS / DEFER — TAG_DEFER was the last one in. classes add no
  new tags. instances ride TAG_OBJECT like every other heap thing.
- **the dispatch shape was pre-designed** (DECISIONS-DELTA, _STRICT): "when
  classes land, operator dispatch reuses exactly this shape — check number
  once, then walk the cheap tag ladder." the perform_* family already has
  the lane. and okvm.c's `perform_add` still carries your TODO, verbatim:

  ```c
  } else if (IS_VALUE_OBJECT(rhs)) {
    goto invalid;
    // TODO
    // not sure if we should allow this for objects or they have to be strictly lhs.
    // TBD after we add classes.
  }
  ```

  C14 answers it. (answer: strictly lhs — see §8.)

## 1. the surface, spec by example

everything below is real oklang syntax as it exists on the errors branch
(semicolons, `let mut`, `fu`, defaults, spans, f-strings) plus the class
additions. frozen lines are yours; the rest is proposal.

```javascript
// decl: class Name : Parent { ... }. the ':' is your ruling; the grammar
// sketch and the lexer's TOKEN_INHERITS say 'inherits' instead — C1.
class Animal : Object {
  let name;                    // field, normal let. no initializer -> null
  let legs = 4;                // initializer runs at construction, in order
  let mut visits = 0;          // mutable field: same 'let mut' as locals

  fu init(name) {              // the constructor body — C3
    super();                   // explicit, always, when a parent has init — C7
    this.name = name;
  }

  pub fu describe() {          // pub = callable from outside; default is private
    return name + " has " + legs as str + " legs";
  }

  fu visit() {                 // private: callable inside the hierarchy only
    visits = visits + 1;
  }
}

class Dog : Animal {
  let breed = "unknown";

  fu init(name, breed) {
    super(name);               // required before touching inherited fields
    this.breed = breed;
  }

  pub fu describe() {          // override: same name, same shape
    return super.describe() + ", a " + breed;
  }
}

let d = Dog.new("rex", "lab"); // construction — C4: new is a meta method
print(d.describe());           // rex has 4 legs, a lab

// statics live on the class object — C9
class Counter {
  static let mut count = 0;
  static fu bump() { count = count + 1; }   // bare 'count' = the static field (see §7)
  pub static fu value() { return count; }
}
Counter.bump();

// operators — C13. the set from your grammar sketch, trimmed where noted
class Vec2 {
  let x; let y;
  fu init(x, y) { this.x = x; this.y = y; }

  pub fu operator +(other) {
    return Vec2.new(x + other.x, y + other.y);   // other.x — see §4
  }
  pub fu operator ==(other) { return x == other.x and y == other.y; }
  pub fu operator str() { return "(" + x as str + ", " + y as str + ")"; }

  pub fu operator [](i) {
    if i == 0 { return x; }
    if i == 1 { return y; }
    throw "index out of bounds";
  }
}

let v = Vec2.new(1, 2) + Vec2.new(3, 4);   // dispatches through operator +
print(v as str);                           // (4, 6) — C13 conversion lane
print(v == Vec2.new(4, 6));                // true
print(v[0]);                               // 1
```

the grammar sketch's `class_decl` / `class_fu_decl` / `class_operator_decl`
already parse all of this (modulo `:` vs `inherits`, C1) — the tree-sitter
side of 0.5.0 is mostly regenerating what you sketched. patch 29 (this
round's only code) is the errors catch-up: `defer_stmt` / `deferr_stmt`,
`finalize` deleted, corpus updated, 132/132.

## 2. rapid-fire — your points, answered

each of these gets its full treatment in the sections that follow; this is
the map.

- **this vs self/me/my — keep `this`** (C2). it is already a token, it is
  the C++/Java/JS word and oklang's object surface is deliberately the
  C++-flavored one (`:` inheritance, `operator +`), and burning `self` now
  keeps it free for a possible module-self later. `me`/`my` are cute and
  alien; cute loses to boring.
- **super always called — yes, with teeth** (C7). explicit `super(...)`
  is REQUIRED whenever any ancestor defines `init`, checked at class
  definition time; it must run before any inherited field is touched.
  when no ancestor defines init, construction is implicit (field defaults
  only) and there is nothing to call.
- **static keyword — yes** (C9). `static fu` / `static let` in the class
  body, storage on the class object, lookup walks the parent chain.
- **ctor name — `fu init(...)`** (C3). `ctor` is oklangcpp accent;
  `__init__` is the python garbage you already rejected; the class-name
  spelling is weird in a language where the body is just members; `new`
  is taken (it is the meta method that CALLS init — see C4). `init` is
  short, a verb, and reads as what it is.
- **dtor — no. ruled out, not deferred** (C8). you already said it better
  than i can: non-deterministic by contract, and defer is the
  deterministic tool (ERRORS.md §12). a dtor would be a second, worse
  way to do what defer does — "why have 2 ways if one do trick?!"
- **conversion `operator OtherClass` — yes, exactly as you sketched**,
  and it plugs into `as` (C13): `v as Vec2` walks classof(v)'s chain for
  an `operator Vec2` and calls it with the receiver. the builtin targets
  are the existing tag names — `operator str`, `operator num`,
  `operator bool` — feeding the same `as` lane that `12 as str` uses.
- **identity operator — it is already in your grammar sketch: `===`**
  (C11). formalize it: `===` is reference identity, total over all
  values, never overloadable. `==` stays value equality, overloadable
  via `operator ==`, and defaults to identity for instances (no
  accidental deep-equal). `!=` is derived, never defined separately.
- **isinstance — a plain function, plus `classof`** (C12). four words,
  four jobs, no overlaps: `is`-family identity (`===`), `as` conversion,
  `==` equality, `isinstance` membership.
- **hierarchy / "one class for builtins right?" — yes, and not just for
  builtins** (C10): ONE metaclass, named `Class`, for every class object
  in the system. no Smalltalk tower. details in §3.
- **fast lane — stays, by construction** (§8/§11): natives are sealed,
  `int + int` never consults a method table, and the perform_* chain
  shape is exactly the pre-designed one. classes are for the methods you
  add later; the primitives keep their lane.
- **native classes like strings — same mechanism, C handlers** (§11):
  a native class object's method table maps names to `ok_native_fn`
  entries (the signature that already exists), receiver passed as
  argv[0]. no fields are ever exposed on natives — their state is the C
  struct. `String.method()` style sugar comes with this for free.
- **operator overloading, c++-like `operator +(_)`, `operator ()(_)` —
  yes** (C13), set trimmed one place: `++`/`--` are NOT overloadable
  (they stay sugar for `x = x + 1`, which dispatches through
  `operator +` with the int 1). the C++ dummy-int trick does not get a
  visa.
- **overload by arity, wren-style — no at the surface, yes underneath**
  (C13): method lookup keys stay (name); operators key (name, arity) so
  unary minus and binary minus coexist. surface-level same-name-different
  arity is rejected — defaults (shipped) and varargs (shipped) already
  cover the real needs, and a third mechanism would be a second way to
  do a solved trick.
- **monomorphic cache from day one — yes, and it is cheaper than it
  sounds** (§12): because method tables are FROZEN at class definition
  (C15 — no reopening, ever), a monomorphic inline cache never
  invalidates. no global epoch, no flush protocol, no write barriers.
  the cache is two pointers per call site.
- **fields, "maybe wren style always hidden" — hidden from OUTSIDE,
  visible INSIDE the hierarchy** (C5). the wren half you want is the
  perf half (no external field syntax at all); the wren half you do NOT
  want is per-class field namespaces that make subclass code unable to
  read parent fields. §4 has the Vec2 argument for why inside-visibility
  is forced anyway.
- **metaclasses get everything — yes, uniformly** (C9): statics are just
  methods whose receiver is the class object. static operators are
  allowed. `Foo.new(...)` is itself just an INVOKE with the class value
  as receiver. one dispatch mechanism total.

## 3. the object model — one metaclass, one loop

the invariant list, then the reasoning:

1. every value is an instance of exactly one class. `classof(v)` returns
   it (a class value).
2. the builtin classes are `num`, `bool`, `str`, `array`, `map`,
   `function`, `nil` — one per existing lane (int and float share `num`,
   matching the `as num` tag; there is no separate `int` class, the int
   representation is a box detail).
3. `Object` is the root of the whole hierarchy. every builtin class and
   every user class sits under it. `isinstance(anything, Object)` is
   true. `Object` has no parent; `super` in Object is a compile error.
4. a class object is an ordinary value: it can be stored, passed,
   returned. `class Foo { ... }` binds `Foo` in the enclosing scope like
   a `fu` decl does (same redefinition rules, same `glob`/`export`
   modifiers where those apply).
5. every class object — builtin and user — is an instance of `Class`.
   `Class : Object`. and `classof(Class) == Class`: the one
   self-instantiation loop in the system, exactly one, at a point you
   can name. this is the whole metaclass story.

why flat rather than a Smalltalk/Ruby tower: a per-class metaclass buys
you per-class method tables you can reopen at runtime — which C15
forbids anyway. what you actually want from "metaclasses" is (a) static
methods and fields with inheritance, and (b) a type for class objects.
(a) is per-class storage + a parent-walk, (b) is one class. the tower
would add a second kind of object whose only job is to hold what the
class object can hold itself. your instinct — "one class for builtins
right?" — generalizes to one class for everything.

what `Object` provides: almost nothing, deliberately. it defines no
methods in v0.5.0 beyond what falls out of the identity rules (`==`
defaults to identity for instances because nobody overrode it). it is
the anchor for isinstance and the place a future `operator str` default
could live if you ever want one. keep it empty until a need is real.

what `Class` provides: the meta surface. `new` (the default constructor
message, C4), and whatever statics user classes define. natives' class
objects are instances of it like everyone else's.

## 4. fields — wren outside, C++ inside

**[FROZEN]** fields are declared with normal `let` (+ `mut` for mutable
ones, + optional initializer). **[PROPOSAL]** the visibility ruling:

- from OUTSIDE the hierarchy, fields do not exist. there is no syntax
  `obj.field` that reaches a field; the instance is opaque; every
  interaction is a method call. this is the wren half — and it is where
  the perf is: with no external field access in the language, field
  offsets are a class-internal detail that can never leak into a
  calling convention.
- INSIDE the hierarchy (the defining class and its subclasses), fields
  are visible on ANY receiver whose runtime class is that class or
  below: `this.x`, bare `x`, and `other.x` where `other` is another
  instance — sibling access, C++-style class-wide privacy.

the second half is not a nicety, it is forced — the Vec2 argument:

```javascript
pub fu operator +(other) {
  return Vec2.new(x + other.x, y + other.y);
}
```

`other` is a Vec2 and the method body needs its guts. if fields were
private per-defining-class the wren way, `other.x` would not resolve and
every operator would need accessor boilerplate — in the one place
(C++-style value types) where operator overloading earns its keep. wren
itself eats this cost; you do not have to.

layout and codegen, the boring parts:

- **append-only, parent-first.** a class's field table is the parent's
  table followed by its own declarations. a subclass can never shift a
  parent's offsets, so parent methods compiled with baked slot indices
  keep working on subclass instances unchanged.
- **redeclaring a parent's field name is a compile error** at class
  definition ("field 'name' already declared by Animal"). with flat
  visibility, silent shadowing would be a footgun; with an error, it is
  a feature.
- `this.x` compiles to a direct slot read/write — a register-indexed
  LOAD/STORE against the receiver, no lookup, no guard (the receiver is
  the defining hierarchy by construction).
- `other.x` compiles to the same slot op behind a one-time class check
  on `other` (is classof(other) within the defining class? walk the
  parent chain; chains are short). v1 walks; a precomputed depth-range
  check is the noted ladder step if profiling ever asks.
- fields with no initializer are `null` at allocation. initializers run
  at construction in declaration order, parent-first (before `init`),
  with `this` bound; reading a field whose initializer has not run yet
  yields null — same discipline as `let` chains today, no new rules.

## 5. methods, this, super

**[FROZEN]** methods are normal `fu` decls in the class body; `pub`
exposes them; the default is private; there is no `private` keyword.

- **private means hierarchy-internal.** a private method is callable
  from the defining class and every class below it; from anywhere else
  it is not visible. same rule as fields (§4) — one privacy doctrine,
  two member kinds. there is no protected; the hierarchy IS the
  protected ring, and the world is pub. (C++'s three-level system is a
  tax on every reader; two levels with a clear line is the oklang
  shape.)
- **overrides are name matches.** same name = override, regardless of
  privacy; `pub` must agree between override and overridden (a pub
  method cannot be quietly privatized by a subclass — compile error,
  the LSP hole you do not want).
- **arity is part of the contract**: an override must accept what the
  parent accepted (same param count after defaults/varargs expansion —
  the shipped function-arity rules, reused verbatim).
- **`this`** is an implicit parameter of every method. it is NOT
  assignable. it is captured lexically: an inner `fu` closes over
  `this` as an ordinary upvalue, exactly like a local — no JS
  dynamic-this disease, no python self-in-closure dance. this falls out
  of methods being ordinary functions with one extra leading register
  (C2 keeps the name; the mechanics were always going to be this).
- bare `x` inside a method resolves: local, then param, then FIELD,
  then the usual outward walk. params and locals shadow fields
  naturally; `this.x` is the explicit disambiguator. assignment to a
  bare name that is a field writes the field (subject to `mut`).
- **`super`** resolves statically against the DEFINING class, never the
  runtime class: `super.describe()` starts the lookup at the parent of
  the class whose method body you are reading. `super(...)` bare is the
  constructor call (C7). `super` is only valid inside a method (or a
  static, where it walks the parent's statics). no `super.super` — one
  hop, re-dispatch from there if you need more.

## 6. construction — new + init, two stages, no dtor

(C4, C3, C7, C8.)

- **`ClassName.new(args)` is the only construction syntax.** `new` is a
  method on the class object (the meta surface), provided by default,
  and overridable: `static fu new(args)` in the class body replaces it
  — interning, pooling, validation-before-allocate, whatever a factory
  needs, in userspace, with zero new machinery.
- the default `new` does, in order: allocate the instance (header +
  class pointer + the layout's field slots, one allocation), run field
  initializers (§4 order), call `init` if the class defines one, return
  the instance.
- **`fu init(...)` is where user construction code lives.** it is an
  ordinary method (privacy rules and all — though a private init means
  only the hierarchy can construct, which is occasionally exactly
  right). it returns nothing; `new` returns the instance.
- **`super(...)` rules** (the "always should be called" ask, with the
  teeth made precise):
  - if ANY class in the parent chain defines `init`, your class MUST
    call `super(...)` explicitly, and MUST do so before reading or
    writing any inherited field. both are compile-time checks at class
    definition — the whole chain is statically known there; this is the
    one place oklang gets java-grade construction discipline for free.
  - if NO ancestor defines `init`, there is nothing to call: implicit
    construction runs the field initializers down the chain and done.
    (requiring a literal `super();` against a parent with no init would
    be ceremony over a no-op — and you already killed ceremony twice
    this year.)
  - `super(...)` with the wrong arity for the parent's init: the
    shipped arity error, at the definition site.
- **no destructor** (C8). GC reclaims instances; deterministic cleanup
  is `defer`, which composes with construction the way you already
  ruled. if a class manages a resource, its idiom is `Foo.open()`
  factories + `defer close()` at the call site — explicit, scoped, and
  already shipped. a dtor would be a second way to do a solved trick,
  with worse timing guarantees.

## 7. statics — the meta surface

(C9.)

- `static fu name(...)` and `static let (mut) name = expr;` in the class
  body. `pub` composes (`pub static fu`). `static` and instance members
  live in separate tables on the class object; a name may not be both.
- **statics are methods whose receiver is the class object.** the
  receiver is spelled `this` inside them too (uniform; `classof(this)`
  is just different). `Foo.bump()` compiles to the same INVOKE shape as
  `d.describe()` — receiver happens to be a class value. there is no
  separate static dispatch path in the VM.
- static lookup walks the parent chain like instance lookup
  (metaclass inheritance without a metaclass per class).
- **bare names inside statics resolve: local, then param, then the
  STATIC fields of the enclosing hierarchy, then the usual outward
  walk.** same ladder as instance methods (§5) with the static table in
  the field slot — one resolution rule, two tables. a static and a
  field may share a name (different tables); a bare read inside a
  static sees the static, inside an instance method the field. if that
  ever surprises anyone, the fix is `this.x`, which works in both.
- **static operators are allowed** — including `static fu operator
  ()(args)`, which makes the class itself callable: `Foo(args)` becomes
  user-defined sugar (typically `return Foo.new(args)`). the DEFAULT
  surface stays `.new` and nothing else — making a class callable is an
  opt-in, and the language never grows a second blessed construction
  spelling.
- static field initializers run at class definition time, top to bottom,
  after the parent class is fully defined. they may reference earlier
  statics and call static methods; forward references are the usual
  compile error.
- static `let mut` is the one shared-mutable-state escape hatch; it is
  per-class (not per-instance), GC-traced, and exactly as dangerous as
  it sounds. fine.

## 8. operators — the set, the lane, the ruling on your TODO

(C13, C14.)

the overridable set, from your sketch, as ruled here:

- **infix**: `+ - * / % ^ & | << >>` and comparisons
  `== != < <= > >=` (`!=` is derived from `==`, never defined).
- **compound assignments** `+= -= *= /= %= ^= &= |= <<= >>= ~=` as the
  IN-PLACE variants: if `operator +=` exists it runs with (lhs, rhs)
  and mutates; if it does not, `a += b` lowers to `a = a + b` through
  the lvalue path (the same non-sugar reality primitives already have —
  DECISIONS on += applies to instances unchanged).
- **unary** `- ~ !` (arity 0 in the (name, arity) table; binary minus
  is the arity-1 entry — this is the whole arity story, C13).
- **call and subscript**: `operator ()(args)` (functors — `f(x)` on an
  instance dispatches), `operator [](i)`, and `operator []=(i, v)` for
  the store side.
- **NOT overridable**: `++ --` (sugar for `x = x + 1` / `x = x - 1`,
  which dispatch through `operator +` with the int 1 — the C++
  dummy-int postfix trick is rejected), `and or not` / `&& ||`
  (short-circuit is control flow, not a method), `===` (identity is a
  language primitive, C11), `=` (assignment), `.` (member access — no
  python `__getattr__`; the method table is frozen, C15).

**left-operand dispatch, only** — the ruling your perform_add TODO asks
for: `a OP b` dispatches on `a`'s class, never `b`'s. if `a` is a
primitive and `b` is an instance, it is a type error — "invalid
operands", the existing message. the idiom for mixed arithmetic is to
write the instance on the left and handle the primitive right side
inside the operator (`Vec2 + int` works, `int + Vec2` does not). this
is wren's rule, it is one lookup instead of two, it composes with the
monomorphic cache (one class to cache), and symmetric dispatch (C++
friend-style both-sided lookup, python `__radd__`) is a second
mechanism for a corner. BORING wins; write your operands left-heavy.

**the fast lane, spelled out** (the pre-designed shape, now with names):

- OP_ADD and friends check `IS_VALUE_NUMBER` first, as today. numbers
  never leave the lane, and nothing can ever intercept them — the
  builtin classes are sealed (C15: you cannot subclass `num`), so "no
  user code between float and float" is not a promise, it is a
  construction.
- the object arm walks the existing tag ladder: STRING+STRING concat
  stays the native lane it is; ARRAY/MAP subscripts stay native; a NEW
  final check — receiver is an instance with `operator +` in its class
  chain — dispatches the method. no existing operand pair changes
  behavior by one bit; the classes arm is strictly new territory at the
  end of the ladder. (this is the _STRICT doctrine applied: the ladder
  is a chain position, the entry check that decides "is this even an
  instance" is the gate.)
- operator call sites get the same per-site monomorphic cache as method
  calls (§12), keyed on the left operand's class.

## 9. conversions — `operator Target`, into the `as` lane

(C13's other half.)

- `pub fu operator Vec2() { ... }` inside class `Vec` defines the
  conversion; `v as Vec2` looks up `operator Vec2` in classof(v)'s
  chain, calls it with the receiver, and returns the result.
- **the target is part of the name**: one operator per (source class,
  target) pair; redefining is the usual compile error. targets are
  class names — including the builtin ones: `operator str`,
  `operator num`, `operator bool` feed the SAME lane `12 as str` and
  `"34" as num` already use, so a user class becomes convertible with
  zero new surface.
- conversions are EXPLICIT only: `as` and nothing else. no implicit
  coercion in calls, no operator-str during concatenation, no
  f-string interpolation hook in v0.5.0 (flagged in C13 — it can be
  added later as "interpolation consults operator str" without moving
  anything else).
- the return value is checked: an `operator Vec2` that returns a non-
  Vec2 is an internal-kind error, not a silent lie. conversions that
  can fail should `throw` (the errors lane composes; `"abc" as num`
  already throws the same way).
- `as` between things with no operator defined: the existing behavior
  (type error), unchanged.

## 10. equality, identity, isinstance

(C11, C12.)

- `===` is reference identity: same value for immediates (the box bits
  for int/float/bool/null — `1 === 1.0` is false, different boxes),
  same allocation for heap values. total, primitive, not overloadable.
  it is already in your grammar sketch; this ruling just promises never
  to make it mean anything else.
- `==` is value equality. for instances it DEFAULTS to identity (an
  untouched class compares by reference — no accidental deep-equal
  surprise), and `operator ==` overrides it pair-wise. `!=` is always
  `not (==)`.
- the existing mixed-type `==` identity fallback (documented in the
  rewrite era, deferred to the error-model round, now shipped) keeps
  its shape: `1 == "1"` is false, not an error.
- `isinstance(v, C)` and `classof(v)` are plain builtin functions, not
  syntax: `isinstance(d, Animal)` walks v's class chain (true for
  Object at the root); `classof(d)` returns the class value.
  `isinstance(x, "not a class")` is a type error. exact-class check is
  `classof(d) == Dog` — `==` on class values is identity, which is
  exactly right for classes.

## 11. natives as classes — String, the handler shape

the builtin class objects (`str`, `array`, ...) are OBJ_CLASS objects
whose method tables hold NATIVE entries instead of closures:

- **the handler is the signature that already exists**:
  `ok_native_fn = ok_value (*)(ok*, uint8_t argc, const ok_value*
  argv)` — with the receiver staged as argv[0]. an instance method
  `pub fu len()` on some class and a native entry both present the same
  call shape to the VM; INVOKE does not care which it found.
- **natives expose methods, never fields.** a String's bytes are the C
  struct; there is no field syntax to expose and never will be. the
  native "layout" is a hardcoded C truth, not a slot table.
- **natives are sealed**: no subclassing (you cannot extend `str`),
  no adding methods from oklang code, no monkey-patching. the method
  tables are populated in C at startup and frozen like everything else
  (C15). the fast lane (§8) depends on exactly this.
- this is the door for "extra methods later like parse": a native
  method is a C entry in the class's table; when a real need shows up
  (`"3" parse`-adjacent conveniences), it is a C-side addition. v0.5.0
  ships the MECHANISM with the existing builtin surface unchanged —
  `as` already covers conversions, per your "we already have as,
  regardless its a keeper".
- the exception to sealing, when it comes: `Exception` (see C16) will
  be the one native class users can subclass, which is precisely why it
  should not be rushed.

## 12. dispatch + the monomorphic cache

(C15 — the load-bearing ruling.)

**method tables freeze when the class definition completes.** all
methods live inside the class body; there is no reopening a class, no
adding methods later, no import-time patching. the compile-time visible
definition IS the definition. consequences, in order of how much they
buy:

1. **the monomorphic inline cache never invalidates.** each call site
   caches (receiver class, method entry); a hit is one pointer compare
   and a direct call. because neither side of that pair can ever
   change meaning, there is no epoch counter, no global flush, no
   barrier protocol — the entire invalidation literature becomes
   irrelevant. this is why "from day one" is cheap: the expensive part
   of inline caching is not the cache, it is the invalidation you skip
   by freezing tables.
2. subclassing cannot break a parent's caches — a new class is a new
   table plus a parent pointer; the parent's entries and layouts are
   untouched (append-only fields, §4).
3. the frozen table is also what makes a future AOT/okc world sane, and
   what keeps `isinstance`/`classof` cheap forever.
4. the cost, honestly: no open classes, no runtime patching, no
   test-framework monkey-mocking. in a language whose modules are
   explicit and whose privacy is real, this is a feature wearing a
   costume. if it ever hurts, the escape hatch is a versioned epoch
   flag added THEN, with the pain in hand — not pre-paid now.

polymorphic call sites (two classes alternating through one site)
re-fill the cache on each flip: monomorphic penalty, exactly as
advertised, exactly what you asked to ship first. the ladder beyond it
(2-3 entry polymorphic slots) is a later-era optimization with the same
frozen-table foundation.

## 13. representation + codegen sketch

no new NaN-box tags (the box is full, §0). two new object types in the
24-bit `info` field (8 of 16M used — room is not a question):

```c
typedef struct {
  object object;            // header: info + gc list link
  object_string* name;
  struct object_class* parent;      // NULL for Object
  uint16_t nfields;                 // layout: parent's fields + own
  string_array field_names;         // for redecl checks + diagnostics
  method_table methods;             // name -> entry, frozen at define
  method_table statics;             // same shape, receiver = the class
  uint16_t flags;                   // native? sealed?
} object_class;                     // OBJ_CLASS

typedef struct {
  object object;
  object_class* class;              // the dispatch key, doubles as type
  value fields[];                   // inline, one allocation
} object_instance;                  // OBJ_INSTANCE
```

- a method entry: {is_native, closure | native fn + arity, defining
  class} — the defining class is what `super` resolves against.
- the method table is an okmap-core instantiation (interned
  object_string* keys, pointer equality, cached FNV-1a — strings
  already carry their hash). the map work of 0.2.0 gets a third user.
- GC: instances trace `class` + the fields array; classes trace name,
  parent, method closures, static storage. the rooting API does not
  grow — class values root like any value.
- `classof(v)` for immediates returns the singleton builtin class
  objects (C statics, immortal like the hidden intrinsics; the GC
  already has the never-collect pattern for these).

new opcodes (sketch, the honest "codegen shape first" pass):

- `CLASS nameidx, parent?` — end of the class-body script: build the
  class object, freeze tables, bind the name. the class body compiles
  as a script that runs once at definition (methods compile to
  closures with baked field offsets, static initializers run inline).
- `GET_FIELD idx` / `SET_FIELD idx` — receiver-relative, baked offset,
  no guard (the `this` case).
- `GET_FIELD_G idx, classref` / `SET_FIELD_G ...` — the guarded variant
  for `other.x` (§4).
- `INVOKE nameidx, recv, argc` (+_LONG) — THE call shape: full lookup
  or cache hit, then the existing call machinery (closure frames /
  native calls), MIC slot keyed by site. statics need nothing new: the
  receiver is the class value.
- `SUPER_INVOKE nameidx, argc` — static chain from the defining class,
  no cache needed (the walk is two pointers).
- operators add the instance arm at the END of each perform_* ladder
  (§8) with the site cache; no new opcode for them.

what deliberately does NOT exist: METHOD (bound-method values — `obj.m`
without a call is not an expression in v0.5.0; lambdas cover the
callback need; flagged in C18), TYPECHECK ops (isinstance is a
function), any field op that takes a name at runtime.

## 14. what we are NOT doing (and why)

- no multiple inheritance (one parent, append-only layout, §4 — the
  whole perf story is downstream of this "no").
- no interfaces / traits / roles this round. single inheritance +
  closures + explicit composition covers v1; if a real diamond-shaped
  need shows up, it gets its own round and its own ruling. (pre-empting
  the question i would ask: no, `pub fu` duck-typing is not a trait —
  it is just dynamic dispatch, and it is fine.)
- no reflection beyond `classof` / `isinstance` (no method lists, no
  field lists, no `responds_to`). frozen tables make these CHEAP to add
  later; nothing needs them now, and "nothing consumed it" is exactly
  how the oklangcpp error-code enums died — recorded in ERRORS.md §8.
- no reopening / monkey-patching (C15 — the cache story).
- no `protected` (§5 — two levels, the hierarchy is the ring).
- no bound methods in v1 (C18).
- no kwargs — deferred AGAIN. the old note said "kwargs deferred to
  classes round"; this round declines them: defaults + varargs +
  spread already shipped, and named-argument dispatch would grow the
  method key from (name) to (name, shape) for one ergonomic win.
  revisit with modules (0.7.0) if the ergonomics still hurt.
- no `async` methods (the grammar sketch parses the modifier; the
  semantics belong to coroutines, 0.6.0, and land with them).

## 15. oklangcpp — the glance that wasn't

i could not reach oklangcpp from the sandbox (codeberg says no such
public repo — renamed, private, or local-only). the one lesson already
banked from the record: its error-code enums died because "nothing ever
consumed the codes, and un-consumed taxonomies rot into lies about the
language" — and this draft applies that lesson to the class surface
(no reflection, empty Object, minimal Class). your "50% broken, 25%
crashing, 25% working" verdict plus "very bad meta classes" is enough
shape to avoid the two classic causes: a metaclass tower nobody could
keep straight (killed by C10 — one flat `Class`), and ctor/dtor pairs
against a GC (killed by C8 — no dtor at all). if you want the real
postmortem section, paste the class files and i will write it against
the actual wreckage rather than the pattern.

## 16. the rulings — C1-C18, OPEN for Q

- **C1** inheritance token: `:` (your message) vs `inherits` (grammar
  sketch + TOKEN_INHERITS already in the lexer). REC: `:`; delete the
  token, it was scaffolding.
- **C2** receiver word: keep `this` (rec) / `self` / `me` / `my`.
- **C3** initializer name: `fu init(...)` (rec) / `ctor` / class name /
  dunders.
- **C4** construction: `ClassName.new(args)` only (rec); default `new`
  overridable via `static fu new`; callable classes are opt-in via
  `static fu operator ()`; no `ClassName(args)` by default.
- **C5** fields: hidden outside the hierarchy entirely; visible inside
  it on any receiver (rec). no `pub` on fields — compile error; methods
  are the surface.
- **C6** method privacy: default private = hierarchy-internal; `pub` =
  everywhere; no third level; pub must survive overrides.
- **C7** super: explicit `super(...)` required (compile-checked at
  class definition) whenever any ancestor defines init; must precede
  inherited field access; implicit construction otherwise.
- **C8** no destructor, ever. defer is the tool.
- **C9** statics: `static fu` / `static let`; class-object storage,
  parent-walk lookup; static operators allowed; receiver uniformity
  (`Foo.m()` = INVOKE on the class value).
- **C10** one metaclass `Class` for every class object; `Class :
  Object`; `classof(Class) == Class` (the only self-loop); builtin
  classes under Object: num, bool, str, array, map, function, nil.
- **C11** `===` = primitive identity, never overloadable; `==`
  overloadable, identity default for instances; `!=` derived.
- **C12** `isinstance(v, C)` + `classof(v)` as builtin functions.
- **C13** operator set as §8 (incl. compounds as in-place variants,
  unary set, `()` `[]` `[]=`; `++`/`--` NOT overloadable); conversions
  `operator Target` incl. builtin targets, into the `as` lane,
  explicit-only, return checked; no interpolation hook in v0.5.0.
- **C14** left-operand dispatch only (your perform_add TODO, answered).
- **C15** tables frozen at class definition: no reopen, no patching;
  MIC from day one with zero invalidation protocol; natives sealed.
- **C16** exceptions stay `{:kind,:what}` maps in 0.5.0; the
  Exception-as-native-class unification (subclassable, the one open
  native) is its own later round with the catch surface migrated
  deliberately; kwargs deferred (again).
- **C17** classes usable anywhere a decl is (top level + nested in
  functions — it falls out of the compiler shape; forbidding would be
  the special case). flagged for a yes/no since it is surface.
- **C18** bound methods (`obj.m` as a value): not in v0.5.0; lambdas
  cover it; revisit with a consumer in hand.

## 17. patch plan (the usual rhythm)

roughly five patches, each gates-green alone, apply-in-order — sizes are
honest guesses, not promises:

1. **30 — class core**: OBJ_CLASS/OBJ_INSTANCE, method tables (okmap
   instantiation), CLASS/GET_FIELD/SET_FIELD/INVOKE(+MIC)/GET_FIELD_G,
   class decl parsing + codegen, `this`, privacy, bare-name field
   resolution. e2e: decl/init/fields/methods/privacy/super.
2. **31 — construction + statics**: default `new`, `fu init`, the super
   ordering checks (compile-time), static fu/let, static initializers,
   `static fu new` override.
3. **32 — operators**: the perform_* instance arms + site caches, the
   operator set, compound lowering, left-dispatch errors, unary set.
4. **33 — conversions + equality**: `operator Target` into the `as`
   lane (builtin targets included), `===`, `operator ==` default-
   identity + override, `isinstance`/`classof` + the builtin class
   singletons bound as globals.
5. **34 — natives as classes**: native method entries in class tables,
   receiver-as-argv[0], the str/array/map class objects with their
   existing surfaces exposed as methods where it already exists as
   builtins (no new stdlib), embed section.

gates at every step: e2e x4 (plain/asan/asan-aggr + the aggressive GC
churn test), unit x3, embed x3, spans corpus diff where bytecode could
wiggle. the bench trio gets a classes-extension (method dispatch +
field access micro) so the MIC numbers are honest from patch 30.

## 18. naming

0.4.0 is Eyrie whenever you tag the errors branch — still my vote, it
earned it. for 0.5.0, from the same well: **Citadel** (the structure
name IS the feature — enclosure, walls, a keep; the album gives it
weight) is my pick; **Urn** is taken by 0.3.0; **Exul** reads too bleak
for the feature that gives the language its shape; **Painters of the
Tempest** is magnificent and too long for a release badge. your call,
there is no bad answer.

— z, session 26. patch 29 (tree-sitter: defer/deferr in, finalize out,
132/132) is in patches/ and download/; nothing else in this round is
code.
