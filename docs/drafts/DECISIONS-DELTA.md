# decisions delta — 26/10 round (owner review of the rewrite + pre-halloween additions)

records the rulings, answers and corrections that came out of the owner's
review round, so DECISIONS.md (oct25) can be amended in place next docs
wave. status vocabulary unchanged: LOCKED / PROPOSED / OPEN.

## D1 — amended: the oracle was for the rewrite, and it retired with it

owner's correction, accepted in full: dorg3 + the old stack VM were the
differential oracle FOR THE PORT (v0.0.4). from v0.0.4 forward the language
is register-only with features the stack VM never had — the old VM can no
longer arbitrate anything, and repro1-3.c are retired as authorities.

one nuance kept: retired-as-oracle ≠ deleted. the repros and the old-suite
differential runs stay in the CI corpus as regression pins (they cost
nothing, and they trip wire on refactors that touch semantics dorg3
exercised). the authority rotates — the halloween game becomes the next
integration oracle, and each generation's game oracles its successors.
"new features land, new bugs emerge, we write tests" is the accepted mode.

## D3 — answered: the rounding question, bitwise, and subscripts

owner asked: is floating-point rounding error real for the
f64-only-with-int48-underneath design? answer, three parts:

1. **not real for integer-domain arithmetic.** f64 is exact for every
   integer |n| <= 2^53 — the entire int48 range fits with room to spare,
   and every int48+int48 and int48*int48 result fits too. the perform_*
   family is already int-aware (int+int produces an int-tagged result with
   zero rounding — okvm.c:243-247), so arithmetic that stays in the int
   domain never touches a double at all. the only places a user can meet
   rounding are genuine fractional math and integer magnitudes past 2^53 —
   neither exists in a 2D game, and neither is a lie: it's f64 being f64.
2. **bitwise, when it lands**: no bitwise opcodes exist yet (tokens do).
   semantics recommendation, parked until they do: JS-style i32 wrap
   (ToInt32), operating on the int payload when int-tagged, on integral
   doubles via trunc, loud error on non-integral/non-number. documented,
   predictable, and it never exposes a type.
3. **subscripts**: int-tagged indices resolve directly; double indices
   resolve iff integral and in range, else loud error. this is the
   int/number duality perform_* already established, formalized in one
   shared VM helper (ARRAYS-V2 §4). one consequence to remember: TAG_INT
   values must never be user-visible as a TYPE — they're a representation.
   if the compiler ever starts emitting int-tagged literals someday (not
   planned), every value-consuming C path (==, print, to_str) must handle
   both representations uniformly. until then, literals stay raw doubles
   and TAG_INT remains internal-only, as it is today.

## D5 — confirmed minimal, now concrete

quick-and-dirty for arrays only, expand when new lvalue kinds arrive. the
concrete shape is in ARRAYS-V2 §5.1: the `lvalue` union gains a
{base, index} subscript arm, resolve_lvalue evaluates both into temps at a
checkpoint above the assignment's own (they must outlive the rhs compile),
store_lvalue emits ARRAY_SET through them. FIELD arrives with classes, at
which point the union grows again — no place-IR rewrite needed before the
second consumer exists.

## D14 — RESOLVED (was: recorded disagreement)

freeze C-style for now; drop it when a real iterator interface lands.
owner's ruling; the parser keeps both forms until that day, for-in lands as
the array special case (ARRAYS-V2 §5.4).

## _STRICT — doctrine corrected: gates strict, chains lenient

the postmortem's "the STRICTLY macros should become THE macros someday"
(lesson #4) is RETRACTED as stated. it was right about the bug, wrong about
the cure. the owner's design intent, now verified in the code:

- a **chain position** — any tag check that runs only after
  `IS_VALUE_NUMBER(v)` returned false — is EXACT without re-checking qnan,
  because a failed number check proves the value is boxed. perform_* is
  built this way (number-gate first, then IS_VALUE_INT / IS_VALUE_OBJECT
  walk, okvm.c:226+): ~70 lenient uses, all chain positions, all correct.
  when classes land, operator dispatch reuses exactly this shape — check
  number once, then walk the cheap tag ladder. this is a designed
  optimization, not debt, and making everything strict would pessimize it.
- a **gate position** — a tag check on a value of unknown provenance — must
  be strict, no exceptions. a raw double whose payload aliases the tag bits
  (19.0, 23.0, 27.0... — any double with sign 0 and mantissa bits 48-49 =
  11) fake-decodes as the corresponding tagged type.

the residual work this doctrine exposes, found this round on v0.0.4 master:

**BUG (public API): `ok_value_as_native_value` (ok.c:229-231) gates on
non-strict `IS_VALUE_OBJECT`.** `ok_value_as_native_value(19.0)` — the
exact aliasing double — passes the gate, decodes payload 0 as an
`object*`, and `object_get_type()` dereferences it. NULL-payload doubles
segfault; other aliasing doubles read a wild pointer. it is the phase-2 bug
class surviving in an embedding boundary that dorg3 never fed an aliasing
double to — another accidentally-correct invariant. fix is one line:

```diff
-  if (!IS_VALUE_OBJECT(p_value)) {
+  if (!IS_VALUE_STRICTLY_OBJECT(p_value)) {
```

plus the same audit glance at any future public ok_* value-inspection
entry point. (value_debug_print in okvalue.c is number-gated first, so its
lenient BOOL/OBJECT checks are chain positions — correct as-is.)

follow-up so intent survives contact with the next refactor: comment the
lenient macros in okvalue.h with their precondition ("exact only when the
value is known boxed — i.e. only after a failed IS_VALUE_NUMBER or inside
a perform_* chain"), so the macros encode the contract instead of relying
on tribal memory. that's the actual lesson: unwritten intent reads as
oversight, and this round it nearly got optimized away.

## `+=` — the "10 minutes" claim, accounted

retracted as a general claim, defended in its original context. when it
was made, every assignable target was a NAME: local (frame register),
global (indexed slot), upvalue (indexed slot). for name targets, compound
assign is genuinely pure sugar — `ADD Rx, Rx, rhs` in place; no lvalue
machinery, no new invariants, an afternoon with tests, hence the parking
lot note. what the claim missed is that arrays make `a[i] += e` the
interesting case on day one — and THERE the complexity is real but lives
entirely in the target discipline (evaluate base/index once, keep them
alive across the rhs, store back through them), which is D5's 3-temp
discipline and is specced in ARRAYS-V2 §5.2. the class case is genuinely
NOT sugar — `operator+=` is a method call with reference semantics, no
store-back, not derived from operator+ — and stays parked with D10.

the owner's lvalue analysis, for the record, was correct in the general
case and dissolves in this VM for a structural reason worth keeping in the
docs: the worry is "the value is materialized in a temp, so += is just +
on a temp". true on a stack machine (everything materializes) and in c++
(rvalues). here, the STORAGE identity is never a temp — locals are
dedicated frame registers, globals/upvalues are indexed slots, subscript
targets hold (base, index) in registers across the RMW. the VALUE may be a
temp; the ADDRESS never is. that's why it's sugar here, and why it would
not be on the old stack VM.

`++`/`--`: rejected for good — `x += 1` exists, prefix/postfix is an
imported footgun. recorded so it stops resurfacing.

## roadmap insertion

pre-halloween sequence becomes: **arrays (ARRAYS-V2) → default args
(DEFAULT-ARGS) → numeric-for → freeze → game.** defaults slot between
arrays and numeric-for because they're small, self-contained, touch only
call() + one opcode + the parser, and the game's function signatures want
them before the game is written. kwargs explicitly deferred to the classes
round (DEFAULT-ARGS §4).
