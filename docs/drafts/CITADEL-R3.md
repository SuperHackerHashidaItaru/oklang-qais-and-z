# oklang 0.5.0 (Citadel) — classes round 3, the closing wall

status: patches 33 + 34, the last two of the classes plan, written against
your classes-staging line (30 + 31 + 32, the state R9–R12 gated green on).
all four of your vetoes this round were confirmations — nothing moved, and
§0 records the one question you bounced back to me (R10's "enforce now?")
with my answer. the round's own load: identity, `is`, the conversions, the
builtin singletons, TAG_AS's funeral, natives as classes, the embedder's C
API — and three real bugs the round flushed out on the way. one of them is
the best receipt in the project's short history (§2).

## §0 — your rulings, applied

- **R9** confirmed: the shadow order stays as shipped — reads resolve
  field-first, call sites method-first. `bare_method_call.ok` still pins it.
- **R10** — you asked "or you think we should enforce it now?" my answer is
  **no, and here is the reasoning i want on the record**: the signature
  contract already holds the line that matters (a hierarchy cannot flip the
  arity of `operator -`, and one-name-one-owner means no silent replace).
  the refinement you sketched — a generated special name for unary, the
  original for binary — is purely internal mangling; it slots into the
  existing enum and the class-resident cache without any table-shape change,
  exactly as you said. so it can land the day a real program wants both
  unary and binary minus in one hierarchy, at one-hunk cost. no such program
  exists today, and paying the name now would be buying a maybe. deferred
  with a map to it, not forgotten.
- **R11** confirmed: the class-resident cache stays, per-site MIC stays
  dead. (your "actually this was my intuition" is logged — the seal was the
  intuition; the cache shape fell out of it.)
- **R12** confirmed: `operator` stays reserved as a field name.

## §1 — patch 33: identity, `is`, classof, the conversions

**`===` is word identity** (C11): total over every value, never
dispatchable, one instruction pair, no refresh needed (it can never nest).
one clarification against the draft's own example, flagged for you as R13
below: the draft said "1 === 1.0 is false, different boxes" — but there is
no public int box. numbers are f64 at the surface (the int tag is internal
machinery; the TAG_INT arm of `as_num` was publicly dead code, see §3), so
`1 === 1.0` is **true**: both are the double 1.0. the visible word-identity
edge is the signed zero — `0.0 === -0.0` is false (the sign bit differs)
while `0.0 == -0.0` stays true (IEEE). both pinned in `identity.ok`.

**`is`** (C12's yes): the exact-class probe — `classof(lhs) === rhs`,
fused into one op, no global involved, never a rebindable consultation.
the distinction to hold onto, pinned in `is_operator.ok`:

```javascript
class Animal : Object {}
class Dog : Animal {}
let d = Dog();
print(d is Dog);     // true   - the box
print(d is Animal);  // false  - `is` answers "what box", not "what family"
print(isinstance(d, Animal));  // true - the family question
print(1 is num);     // true   - where the guard form reads right
```

**`isinstance` + `classof`** are plain natives, rebindable like `len` (the
primitive lanes — `is`, `===` — never consult bindings; that was the point
of fusing). `classof(null) === nil`, `classof(Class) === Class` (the one
self-loop), every class value is `is Class`. two judgment calls are now
code: native values sit directly under Object, bound methods count as
function.

**`operator <Target>` rides the `as` lane** (C13): an instance probes its
chain first — conversions inherit, private ones are hierarchy-internal, and
the result is checked (a target or any descendant of it is legal; anything
else is an internal error, "returned the wrong type"). builtin receivers
skip the probe entirely (sealed classes cannot carry user operators), so
`12 as str` stays allocation-free. no operator and no builtin lane is the
honest `"no conversion from 'Vec' to 'array'."` an instance without
`operator str` keeps the `<instance Vec>` default; with it, the method
decides.

**the seven singletons** (C10): str/num/bool/array/map/function/nil, all
under Object, seeded as top-level values through ONE table both sides of
the seeding walk (the old two hand-built arrays were position-paired by
discipline alone — the kind of thing that drifts). `nil` is null's class
and null is its sole instance; `isinstance(null, Object)` is true through
the chain, exactly as the draft promised.

**the funeral**: TAG_AS, TT_STR/TT_NUM/TT_BOOL, the hidden `" str"` /
`" num"` / `" bool"` globals — all dead. the `as` target is a real class
value now, read like any other operand. tag id 6 stays reserved so the
survivors keep their numbers; the unit test pins the numbering.

## §2 — the guard-stack bug (the receipt of the round)

the chase, told short. the aggressive-GC lane failed
`conversion_private_error.ok` as *"expected error, got success"* — but only
in suite runs, never solo, and the failing set changed between runs.
invocation shape: nope. time of day: apparently. instrumentation: the
conversion probe printed `'operator num'` while the target pointer WAS the
str singleton — the str singleton's **name object had been swept at
startup, and its memory reused by the num name**.

the cause: `vm_init_singletons` held every startup name on the gc guard
stack at once. `OK_GC_GUARD_CAPACITY` is **16**. the sequence was already
at 17 pushes before this round (the 17th silently failing, harmless only by
luck of ordering); the seven builtin names pushed it to 24. gc_guard_up
returns false past capacity — its own comment says callers must treat that
as "could not root", *never ignore* — and nobody checked.

the fix: per-object balanced guards. a name rides the stack only across
the class creation that adopts it; the assignment roots the pair and the
class's trace roots the name, so the drop is immediate. the cached lookup
names need no guard at all — their assignment into the vm IS the rooting
(nothing allocates between the two). every push is checked now, and the
discipline is documented at the function so the next reader inherits it.

## §3 — the other receipts

1. **the asprint leak**: every object_as_str arm (class, instance, bound
   method, closure, native fn, native value) leaked its printed
   intermediate buffer. lsan found it the day `instance as str` first ran
   in the corpus — the object arms had simply never been reached before
   classes existed. `as_str_printed` releases after the copy; map and
   array already did it right and were the template.
2. **as_num's tail**: a missing `return true` after the int arm (publicly
   dead — no int box surfaces — but a landmine one refactor away from
   live), a "to string" copy-paste in the fallback message, and no
   instance/class/method arms at all. fixed; `Plain() as num` now says
   "instance is not convertible to number."
3. **a test bug worth confessing**: `operators_subscript.ok` mixed
   `// expect:` lines with an `// expect error:` line, and the full runner
   treats an error-directive file as error-lane only — its output lines
   were never compared, hiding a wrong expect (40; the correct answer is
   45). split into two files, expect corrected. the quick runner and the
   full runner agree on it now.

## §4 — patch 34: natives as classes (the tough one, tamed)

you called it the tuffest. it turned out to be the best-prepared patch of
the five — the door was pre-built in p30's `method_entry` (the native
fields were waiting), and the window shapes were already aligned.

- **one flight, two shapes.** `run_native` is the shared core: depth-
  tracked, both error lanes handled, and a bounds check against
  ok_native_fn's 8-bit argc that closes a pre-existing landmine — the old
  call_native would truncate a 256+ argument count and then overrun its
  stash on an ANY-arity native. `call_native_method` is the method shape:
  the receiver rides argv[0], and the INVOKE window, the bound-method
  window, and the operator-dispatch staging turn out to be the same
  window — which is why the whole thing is small.
- **all seven traps became lanes**: INVOKE, SUPER_INVOKE, bound methods,
  the instance call operator, the static call operator, the operator
  dispatch, and a C-defined `init` (receiver-first flight over the stashed
  construction args, result discarded — the construction's value is the
  instance, same as the closure lane).
- **the receiver lane**: `a.len()` maps builtin receivers to their class
  singleton through classof, mic-cached like any site (sealed tables make
  it sound, same as everything else). and the missing-method error on a
  native receiver now names the class: `"class 'str' has no method 'len'."`
- **the surfaces, no new stdlib** (your ruling, held by construction): the
  C handlers were receiver-first functions all along — `len(x)` always
  took its receiver as argv[0] — so the SAME entries mount twice. array:
  len/push/pop/remove. map: len/remove/get. `len(a)` and `a.len()` are one
  function. str/function/num/bool/nil carry nothing in 0.5.0: nothing
  existed as a function builtin to mirror. (R15 below.)
- **the embedder's C API**: `ok_class_new` (unfrozen; a class parent or
  null for Object), `ok_class_define_native_method` (arity counts the
  receiver, pub flag), `ok_class_freeze` (the seal), `ok_value_is_class`.
  the definition rules are the script lane's, enforced C-side: one name
  one owner, pub survives overrides, no private override of a pub
  ancestor, and the signature contract — where a native's declared arity
  counts the receiver and a closure's parameter count does not, both
  express "the argc this entry dispatches at", and overrides must agree on
  that quantity.
- **the embed test grows a Math class**: constructed from script, methods
  called, a method bound as a value, isinstance against Object, a
  mic-warmed 100-iteration loop, a native `operator +` entry dispatched
  through the binary ladder, privacy from outside the (empty) hierarchy
  ring, redeclaration refused, post-freeze definition refused.

## §5 — what remains of 0.5.0

- **patch 35 (proposed, still awaiting your word)**: the copy-on-redefine
  from R2 §5 — `make_class()` handing out a fresh class per call. the
  analysis stands, and this round made it cheaper than it was at R2: the
  conversions ride runtime probes, not freeze bakes, so the proto-copy
  would rebase against less. say the word and it lands as its own small
  patch.
- the small-optimization round you re-scheduled after classes close: the
  benchmarks (delta blue; wren added to the matrix) belong there, next to
  the profiling — per your own conditional, this round was not small.
- tree-sitter catch-up: `is` and `===` tokens, the classes grammar
  regeneration. external repo, one patch, whenever you want the corpus
  current. flagged so it does not get lost.
- later, per the standing rulings: Exception as the one subclassable
  native (C16), kwargs (deferred again), the interpolation hook consulting
  `operator str` (C13's flagged maybe).

## §6 — open for your veto (R13–R16)

- **R13 the `===` box clarification**: accept that `1 === 1.0` is true
  (no public int box exists; the draft's example sentence was written
  against a surface int/float split that never shipped), with `0.0 ===
  -0.0` false as the deliberate word-identity edge. the alternative is
  special-casing the signed zero or waiting for a real int surface — i
  recommend accepting the clarification as-is.
- **R14 `is` as exact-class**: `d is Animal` is false; the chain walk
  lives in isinstance. this is the R1-recorded ruling, now pinned. the
  one-hunk alternative is a chain walk inside the op — i recommend
  keeping exact; `if x is num` is the shape it reads as, and the family
  question has a named tool.
- **R15 the empty str surface**: str carries no methods in 0.5.0 because
  nothing existed as a str function builtin to mirror and the ruling is
  no new stdlib. confirm, or name what you want on str and it becomes a
  one-line table entry plus a test (`len` is the obvious candidate, but
  it would be new stdlib by the ruling's letter).
- **R16 patch 35**: the copy-on-redefine. yes/no.

## §7 — applying

`git apply patches/33-conversions-identity.patch` then
`patches/34-natives-as-classes.patch`, in order, on top of 30+31+32.
verify: `build_ok.sh all` + `asan-aggr`, then the full runner — the new
floor is **394 x4 + unit x3 + embed x3**. the new suite:
`classes/identity`, `is_operator`, `classof_isinstance`,
`builtin_classes`, `conversions`, `native_methods`, the six new error
tests, and the subscript split. formatting hand-matched as always; your
clang-format remains the canonical pass.

— z, session 29. Citadel: the wall is up. what's left is a door (35) and
the polish rounds.
