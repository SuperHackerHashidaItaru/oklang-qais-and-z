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

## session-10 addendum — 26/10 (evening): the arrays/defaults round, rulings in

owner verdicts on this wave's drafts, recorded as delivered:

- **D4 amended (len)**: `len` is a native function, NOT an opcode. owner's
  pushback accepted: the opcode budget is real (54/128 used before arrays;
  maps + classes + numeric-for are still owed), len is cached at loop entry
  by the for-in desugar anyway, and a native extends to strings/maps later
  with zero bytecode changes. cost: the for-in desugar depends on the `len`
  global by compile-time index — `len` is effectively a reserved name until
  the iteration protocol retires the dependency. documented in the spec.
- **for-in syntax**: `:` not `in` — c++ style, no new keyword. the colon
  family: `for let v : arr? {}` (values from a sequence) and, when numeric-for
  lands, `for i = 0 : n? {}` (numbers from a range). collision audit clean
  (ternary is then/otherwise; map literals are brace-context).
- **remove native**: `remove(a, m, n)` — half-open [m, n) at the language
  surface, translated onto the inclusive-end OK_ARRAY primitive. the reasons
  half-open is right: n-m is the count, empty is m==n, adjacent ranges share
  an endpoint, loop bounds compose. (dijkstra's ewd831 argument; the owner's
  memory of "there was a benefit, i swear" was correct.)
- **safe-get native: ruled out.** out-of-bounds access must stay a hard error
  (explicit over implicit is the owner's doctrine, not mine to soften); the
  idiom is `if i < len(a) then a[i] else null otherwise ...`-shaped until
  methods exist. these four natives are stubs for methods anyway.
- **defaults: accepted as specced** — call-time, trailing-only, no skipping,
  "illegal period". kwargs: later, compile-time-resolved-only if ever.
- **numeric-for: draft requested + delivered** (docs/drafts/NUMERIC-FOR.md).
  open questions listed there (step syntax, frozen limit, direction strategy).
- **patch-the-array follow-up logged**: when numeric-for lands, for-in's
  counter tail (LESS+JUMP_IF+ADD+LOOP, 5 dispatches/iter) switches to FOR_LOOP.
- **v0.0.4 tag**: owner will delete + retag with the ok.c fix folded in
  (zero consumers; patch-slot-for-youth versioning is intentional and fine).

process notes from the implementation itself:

- the gc-guard memorial earned its keep again: OP_ARRAY_NEW initially skipped
  the guard on the reasoning "append_n only reallocs" — wrong, the gc-aware
  allocator collects on EVERY reallocate growth, so the fresh array was swept
  between linking and publication. aggressive GC caught it on all 19 array
  tests; guard window added; lesson unchanged, just re-learned.
- latent parser bug fixed in passing: `fu f(b = 5)` used to parse the assign
  into the binding's lvalue and the compiler cast it to an identifier —
  silent garbage, never covered by a test. param names now parse at
  PREC_ASSIGNMENT and are validated as identifiers.

## round 11 addendum (numeric-for + disassembler + assign peephole)

- **direction strategy resolved**: zero extra enum slots. one fused FOR_LOOP
  with a per-iteration step-sign test (lua 5.4 float-loop shape). the 2-slot
  UP/DOWN option was analyzed and rejected: serial back edges cost the same
  sign test for positive loops and strictly more for negative; entry-selected
  back edges need body duplication or self-modifying bytecode, and
  self-modifying breaks re-entrancy (recursive function, differing loop
  direction per frame, resumed on a rewritten back edge). the owner
  pre-authorized the slots; spending none is the better deal.
- **`let` required in the numeric for head** — consistency ruling from the
  owner's review. the draft's optional let had no good reason and created a
  shadow-vs-assign ambiguity. the whole for family now reads
  `for let (mut)? IDENT ...`.
- **exclusive bounds, both directions**: up runs while i < limit, down while
  i > limit. symmetric, matches the `i < n` idiom the game's loops use.
- **adjacency encoding**: FOR_PREP/FOR_LOOP read limit at counter+1 and step
  at counter+2. forced by the 2-word layout (offset takes one 16-bit slot,
  three registers do not fit the other); the compiler declares the three
  adjacently and asserts. documented in okchunk.h.
- **PREP does the first test directly** (no lua pre-subtract): start - step
  overflows to ±inf and infinite-loops at f64 extremes; the direct test is
  exact there.
- **NaN bounds = empty loop** (all comparisons false). documented, not
  special-cased.
- **disassembler pre-poned from v0.0.6** (owner's call, buffer day): per-op
  decode, jump targets, constants inline, proto recursion, `ok --dump`.
- **local_pressure theory REFUTED by inspection**: constants already fold
  into binop operands; no LOADK-before-ADD exists post-rewrite. the real cost
  was assign's scratch-temp + move-back pair.
- **assign peephole shipped**: `local = <simple-left> op <rhs>` compiles the
  rhs straight into the local's register (ADD r2 = r2 + k, no temp+MOVE).
  gate is load-bearing: left operand must be identifier/literal —
  `a = (a + 1) + a` and `a = f() + a` re-read the variable after the left
  side may stage state in the destination, so they keep the scratch path
  (both have regression pins). operands are read at op time (pinned by
  assign_operand_read_timing.ok: a mutating call on the right observes the
  mutation before the add).
- **materialize_value skips self moves** (register already in dest) — pure
  win, may remove other redundant moves.
- parked, logged: compound-assign (`a += 1`) has the same temp+move shape
  (natural follow-up); MOVE staging before ARRAY_GET/CALL arguments is the
  register-window convention (bigger design question, not a peephole).
