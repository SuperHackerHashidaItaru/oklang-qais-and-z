# spec: default arguments (`fu f(a, b = null)`) + the kwargs verdict

date: 26/10. owner's proposed pre-halloween addition, reviewed against
v0.0.4 master. verdict up front: **yes, doable and genuinely cheap — do it
this window, after arrays, before numeric-for. kwargs: later, and not the
python kind.** the reason it's cheap is structural: all the hard
prerequisites already shipped in v0.0.4 as bug fixes.

## 1. surface

```ok
fu greet(name, greeting = "hello", punct = "!") ?
  print greeting + " " + name + punct;

greet("qais");                     // hello qais!
greet("qais", "yo");               // yo qais!
greet("qais", "yo", "?");          // yo qais?

fu scale(v, factor = 1) ?          // numbers
  return v * factor;

fu make_pair(a, b = a) ?           // earlier params visible — legal, call-time
  return [a, b];

fu fresh(box = []) ?               // FRESH array per call — see §3, this is the test
  return box;
```

rules:
- defaults are trailing-optional by position: a call may omit only a suffix
  of the parameter list. `f(, 2)` is a syntax error, today and forever.
- any expression allowed as a default, including calls — evaluated at CALL
  time, only when the argument is actually missing.
- arity check becomes a range: `arity - ndefaults <= argc <= arity`. below
  the floor: same loud error as today ("expected at least N arguments, got:
  M"); above the ceiling: unchanged.
- natives (object_native_function) keep exact arity in v1. embedders who
  want optional behavior write two globals or take null explicitly — the
  dorg3 binder already lives this way.

## 2. mechanics (what actually changes)

**already on master — the two things that made this a design question in
most VMs:**
- `call()` null-fills the callee's unwritten window (`first_unwritten =
  callee + argc + 1 .. frame_top`, okvm.c:1557-1565, the phase-5 stale-window
  fix). missing params therefore arrive as tagged nulls, GC-safe, before any
  bytecode runs. the defaults ladder just overwrites them. without this
  pre-existing loop, defaults would be a GC bug factory.
- the call window convention (`r[a] = r[a](r[a+1]...)`) means the callee
  never inspects how it was called — only `argc` matters, and only the
  callee's own prologue cares. no call-site changes at all.

**new, in order of size:**

1. `ast_binding` gains `ast_expression* default_value` (NULL = required).
   parser: in the param list, after a binding, `= <expr>` attaches it.
   enforce "defaults must be trailing" at parse time (a required param after
   a defaulted one = syntax error with a note).
2. `object_function` gains `uint16_t ndefaults` (arity stays TOTAL params —
   the runtime computes the floor as `arity - ndefaults`).
3. `call_frame` gains `uint16_t argc`, set by `call()` — this is literally
   the D6 field ("argc becomes a runtime field set by call()"), landing
   early with its first consumer. varargs later builds on it unchanged.
4. `call()` arity gate (okvm.c:1522) becomes the range check + the error
   message learns "at least".
5. one new opcode, the guard:

```
OP_JUMP_IF_ARGC   2w:  (p, A, B)  ->  ip += A when (p == 1) == (frame->argc > B)
```

   same relative-offset semantics and patch tooling as OP_JUMP_IF
   (emit_incomplete_jump / patch_jump_if shape), condition sourced from
   frame->argc instead of a register. B is the 0-based PARAMETER position.

6. compile_function_impl emits the ladder as the function's first code,
   after params are declared, before the body:

```
       for each defaulted param i (positions arity-ndefaults .. arity-1):
         OP_JUMP_IF_ARGC  +offset_i, i      // arg supplied -> skip its default
   L_i:  <compile default expr into r[param_i]>   // earlier params in scope
       ... body ...
```

   each default expr is an ordinary compile_expression into the param's own
   register; temps behave normally (ladder runs before body code; per-expr
   checkpoints already reset). side-effectful defaults fire only when the
   argument is missing — the guard skips exactly that param's block.

cost accounting: functions without defaults emit nothing and check one
compare in call() (unchanged). functions with defaults pay one guard
dispatch per defaulted param on full calls, and the expr when actually
filling. call sites, the CALL opcode, closures, dynamic calls through
variables — zero changes. that's the whole feature.

## 3. why CALL-time evaluation (the one real decision)

python evaluates defaults once at def time; c++ at each call. we take c++,
for two reasons that both bite the day arrays land:

1. `fu fresh(box = [])` under def-time semantics shares ONE array across
   all calls — the classic python footgun, live ammo in a language whose
   pre-halloween game plan is "fake structs with arrays". call-time gives a
   fresh literal evaluation per call. no exception to remember, no docs
   warning to write.
2. call-time is the only semantics under which `b = a` / `b = a * 2` can
   work at all (python can't reference other params in defaults precisely
   because they don't exist yet at def time). earlier params are already in
   their registers when the ladder runs — it falls out for free.

the cost (re-evaluating constant-ish defaults per call) is a dispatch or
two; if it ever shows up in a profile, constant defaults can be hoisted
into a fill-table fast path later WITHOUT semantic change (a constant
default's fill is indistinguishable from evaluating it). no premature
machinery now.

## 4. kwargs — now or later?

**later. and when they land, they are not python's.**

- what we would add: named call sites — `draw(10, 20, scale=2)` — resolved
  to positions. nothing else. no `**spread`, no dict↔params coupling, no
  keyword-capture (`def f(**kw)`), no positional-after-keyword gymnastics.
  naming what you pass is explicit; the rest of python's kwargs culture is
  dynamic-typing machinery we already rejected the analogues of.
- the cheap implementation only exists for DIRECT calls (callee is a
  statically known fu): the compiler maps name→position at compile time and
  emits an ordinary positional call — zero runtime cost, zero protocol. but
  the whole point of the owner's closures-as-objects pattern is calling
  THROUGH values (`counter("inc", 1)`), where the callee is unknown at the
  call site. kwargs through dynamic callees need a runtime name-matching
  protocol (pack (name, value) pairs, prologue match) — real complexity,
  and pointless before classes exist to give it a home.
- classes (D10) are where kwargs earn their keep: named constructor
  arguments over frozen shapes. design them together, once, with `let
  unset_member` definite-init and the ctor story — not twice.
- relationship to closures-as-objects: none, and that's the point.
  `counter("inc", 1)` is data-driven dispatch (a string IS data — it can be
  computed, stored, forwarded). `f(x=1)` is static parameter naming. two
  different layers; the game will use the first heavily and not miss the
  second until it has classes.

sequencing verdict: defaults land pre-halloween (this spec). kwargs wait
for the classes round, arrive as compile-time-resolved named call sites
first, dynamic-callee protocol only if a real case appears.

## 5. tests

- defaults/: basic fill, multiple fills, all supplied (guards all taken),
  none supplied beyond required, expr-default (a * 2), default referencing
  earlier param, fresh-mutable-default (`fu f(a = [])` called twice,
  push to one, other stays empty — THE §3 test), default with side effect
  + counter proving it runs only when missing, lambda with default,
  closure-in-variable called with missing args (dynamic callee path).
- defaults/errors/: too few (below floor — message says "at least"),
  too many (unchanged), non-trailing default (syntax error).
- matrix: all four build variants; the fresh-mutable-default test under
  aggressive GC (default expr allocates an array inside the prologue —
  exercises ladder temps + collection mid-prologue).
