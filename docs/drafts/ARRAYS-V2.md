# spec: arrays v2 (implementation-grade — supersedes docs-oct24/specs/arrays.md)

date: 26/10. target: land on master (v0.0.5 pre-halloween scope, day 1 of 3).
everything below is written against v0.0.4 master (`1697066`) — file names,
function names and opcode layouts are real, verified this session. read this
with okchunk.h's layout comments open.

status of the scaffolding (already on master, nothing to re-create):
- tokens: TOKEN_LEFT_BRACKET / TOKEN_RIGHT_BRACKET / all compound-assign
  tokens exist (oktoken.h).
- parser: `[` has a pratt slot `{NULL, NULL, PREC_SUBSCRIPT}` — infix handler
  is the only missing piece (okparser.c:119).
- AST: AST_ARRAY_EXPRESSION / AST_SUBSCRIPT_EXPRESSION /
  AST_COMPOUND_ASSIGN_EXPRESSION / AST_ACCESS_EXPRESSION exist in the enum;
  their X-macro entries in okast.c are commented out (structs not generated).
- compiler: `lvalue_kind` already declares LVALUE_SUBSCRIPT / LVALUE_FIELD;
  `expression_type` declares EXPRESSION_SUBSCRIPT; `store_lvalue` has the
  switch with a CRASH default; `resolve_lvalue` handles identifiers only.
- parser's `is_lvalue()` (okparser.c:522) already accepts
  AST_SUBSCRIPT_EXPRESSION for assignment targets.

so arrays are a fill-in-the-blanks feature, not new architecture. that is the
headline.

## 1. surface (unchanged from oct24 + D4 deltas)

```ok
let a = [1, 2, 3];
let mut m = [0, 0];
m[0] = 5;
m[0] += 7;                    // compound store — same target discipline
print a[1];
push(m, 42);
let last = pop(m);             // null on empty (locked in D4)
print len(m);
for let v in m ?
  print v;
```

- indexing 0-based, number-only. int-tagged accepted directly; double accepted
  iff integral and in range (the perform_* duality — see §4). negative or
  non-integral or wrong type = loud runtime error with file:line:col.
- OOB load AND store: `"index out of bounds: 5 (len 3)"`.
- `==` on arrays: identity, like every non-string today.
- no slicing. no `[n; fill]`. no `+` concat (concat is a native if the game
  needs it — recommend NOT adding it until asked; strictness doctrine).
- mutation during for-in is allowed and reads the mutated array; len is
  cached at loop entry (growing mid-loop does not extend the trip; shrinking
  below the counter errors loudly at the next ARRAY_GET — documented).

## 2. representation

```c
// okobject.h — mirror object_string's shape
typedef struct {
  object object;
  value_array values;   // ARRAY_DECLARE(value_array, value, uint32_t) already exists (okvalue.h:137)
} object_array;

object_array* create_object_array(uint32_t p_initial_capacity, object_specs* p_specs);
```

- new enum value OBJ_ARRAY in `object_type` (append AFTER
  OBJ_NATIVE_FUNCTION — the enum order is load-bearing for nothing we know
  of, but append anyway; object_dispatch_deinit gains the case: values_deinit
  + nothing else, the struct is one allocation).
- values.data is a separate realloc'd buffer holding tagged values. realloc
  is always safe: elements are NaN-boxed handles, nobody holds interior
  pointers into `values.data` — only the object_array owns it. this is why
  arrays and this GC compose with zero barrier work while it stays
  stop-the-world (write barriers arrive with incremental GC, v0.2.x).
- growth: `array_grow_capacity` (cap<8 → 8, else *2) via `value_array_grow`
  — same curve as the register file. count is uint32.

**allocation order (the trace_object bug class — phase 2, the whole reason
this section exists):**

```
create_object_array(len):
  1. alloc the object_array struct (object_store_add — a collect can fire HERE)
  2. values_init(&arr->values)              // count=0, cap=0, data=NULL
  3. if len > 0: value_array_grow(len)      // alloc can fire a collect too
     + null-fill values.data[0..len)        // count = len
```

step 1 leaves the struct unrooted (not yet in any register) while step 3 can
collect. therefore OP_ARRAY_NEW and push() must run with the fresh array
**gc_guard_up'd** until it is published into its destination register /
already sitting in an argument register. copy the exact pattern from
compile_function_impl (okcompiler.c:2290: guard proto, build, publish, guard
down). one guarded region per allocation site, no exceptions, and the
aggressive build is the test (the 300-element literal test below exercises
grow-under-collection).

## 3. opcodes (against the real layouts)

narrow = `op p a b c` (8-bit), wide = `op p a' b'` (12-bit), 2-word =
`op p A B C q` (16-bit + q). house rule observed: p/q select
constant-vs-register for the operand they sit on; LONG variants use the
OP_ADD_LONG dual-flag shape.

| op | form | operands | semantics |
|----|------|----------|-----------|
| OP_ARRAY_NEW | w | (a', b'; p) | `r[a'] = new array; count = p ? k[b'] : r[b']` (null-filled) |
| OP_ARRAY_NEW_LONG | 2w | (p, A, B) | same, wide |
| OP_ARRAY_GET | w | (p, a, b, c) | `r[a] = r[b][ p ? k[c] : r[c] ]` |
| OP_ARRAY_GET_LONG | 2w | (p, A, B, C) | `r[A] = r[B][ p ? k[C] : r[C] ]` (q unused) |
| OP_ARRAY_SET | w | (p, a, b, c) | `r[a][ p ? k[b] : r[b] ] = r[c]` — dest order matches OP_MOVE family; value is a register in narrow (materialize first) |
| OP_ARRAY_SET_LONG | 2w | (p, q, A, B, C) | `r[A][ p ? k[B] : r[B] ] = (q ? k[C] : r[C])` |
| OP_ARRAY_LEN | w | (a', b') | `r[a'] = count(r[b'])` as f64 (D3: never emit int tags) |
| OP_ARRAY_LEN_LONG | 2w | (A, B) | same, wide |

encoding notes:
- SET narrow has no room for a value-const flag (p is spent on the index) —
  fine: constant values materialize through a temp or use the 2w form. the
  literal compiler (§5) always knows the count ahead and uses 2w SETs with
  q=1 when elements are constants, so `[1, 2, 3]` costs 1 NEW + 3 SETs and
  zero temps.
- GET/SET bounds-check with one compare each against the live count. type
  check of the base (`IS_VALUE_STRICTLY_OBJECT` + `object_get_type ==
  OBJ_ARRAY` — go through a `value_is_object_type`-style strict helper, see
  the decisions-delta doc on gates vs chains) errors with
  `"invalid operand for '[]': expected array."`.
- index resolution is ONE shared VM helper (§4). do not fork it per opcode.

## 4. index resolution (the D3 duality, made precise)

```c
// returns true + *out_index on success; false after calling runtime_error
static bool resolve_array_index(vm* p_vm, value p_index, uint32_t p_count, uint32_t* p_out);
```

- `IS_VALUE_INT(index)` → payload, compare against count. (chain position is
  fine here IF the number check ran first — see ordering below; keep this
  helper strict-gated since it is called from opcode handlers on arbitrary
  operand values: number-gate first, exactly like perform_add.)
- else `IS_VALUE_NUMBER(index)` → `double d = VALUE_AS_NUMBER(index);`
  accept iff `d == trunc(d) && 0 <= d && d < count`.
- else `"array index must be a number, got <type>."` (null/string/bool all
  land here — loud, uniform).
- negative integral: `"index out of bounds: -1 (len 3)"` (same message as
  OOB — it IS out of bounds; don't invent a second message).
- non-integral: `"array index must be a whole number, got 1.5."` — distinct
  message, because "silently truncating" is the alternative we are refusing.

why integral doubles are accepted at all: numeric literals are raw doubles
(D3), so `m[0]` arrives as a double 0.0. rejecting doubles would make every
literal index an error. accepting only integral ones keeps `m[1.5]` loud.
this matches the existing int/number duality inside perform_* and costs one
`trunc` compare on the double path only.

## 5. compiler

### 5.1 lvalue extension (D5's minimal version, now concrete)

`lvalue` union gains the subscript arm; nothing else changes shape:

```c
typedef struct {
  lvalue_kind kind;
  union {
    location variable;
    struct { location base; location index; } subscript; // NEW
  };
} lvalue;
```

- `resolve_lvalue()` gains `case AST_SUBSCRIPT_EXPRESSION`: compile base into
  a temp, index into a temp — **at a checkpoint above the assignment
  expression's own temp checkpoint**, because these two registers must
  survive compilation of the right-hand side (compile_expression resets
  temps to ITS checkpoint on exit — okcompiler.c:654). this is the 3-temp
  discipline from D5, now with a line number: base, index, value.
- evaluation order is thereby pinned left-to-right: base, index, rhs. `a[f()]
  = g()` calls f then g. document it in the spec, test it.
- reads: compile_subscript as an expression returns
  EXPRESSION_VALUE|EXPRESSION_SUBSCRIPT with base/index locations and emits
  ARRAY_GET into p_dest.
- writes: `store_lvalue` gains `case LVALUE_SUBSCRIPT` → materialize value
  into a temp if not already a register (narrow SET needs a register value)
  → emit ARRAY_SET with base/index reused.

### 5.2 compound assign (`+=` and family) — the desugaring table

AST_COMPOUND_ASSIGN_EXPRESSION {op, left, right}. compile as
read-modify-write through the SAME lvalue:

| target | desugaring | ops |
|---|---|---|
| local `x` | `ADD Rx, Rx, rhs` | 1 (in place — locals ARE frame registers, there is no temp to worry about) |
| global `x` | `LOAD_GLOBAL rt; ADD rt, rt, rhs; STORE_GLOBAL` | 3 |
| upvalue | same shape via u[] ops | 3 |
| `a[i]` | base/index evaluated ONCE into temps; `ARRAY_GET rt; ADD rt, rt, rhs; ARRAY_SET` | 3 + addressing |
| class instance (later, D10) | `lhs.operator+=(rhs)` method call — NOT derived from operator+, not sugar, reference semantics, no store-back | 1 call |

why the "lvalue-ness" question dissolves in this VM: the worry (from the c++
mental model) is that the loaded value is a temporary, so `+=` would just be
`+` on a temp. correct worry, wrong machine: here the STORAGE identity is
never a temp — locals live in dedicated frame registers, globals/upvalues in
indexed slots, and subscript targets keep their (base, index) in registers
across the RMW. the value being a temp is fine; the address never is. that
is the entire justification, and it is also why this lands as part of arrays
(the first non-name target) rather than before them.

parser: switch TOKEN_PLUS_EQUAL..TOKEN_SHIFT_RIGHT_EQUAL table entries from
`parse_binary_infix` (their current placeholder binding at PREC_ASSIGNMENT)
to a compound-assign handler that validates is_lvalue(left) and builds the
node. arithmetic compound ops only for v1 (`+= -= *= /=`); bitwise compound
tokens stay reserved until bitwise ops exist at all (no OP_BITAND in the
enum yet).

`++`/`--`: not doing them. `x += 1` exists; prefix/postfix distinction is a
footgun we have no obligation to import. (recorded so it stops coming up.)

### 5.3 literals

`[e1, ..., eN]`:
1. allocate dest temp;
2. `OP_ARRAY_NEW dest, N` (const count, guarded publication per §2);
3. per element: compile `ei` (constant → LOCATION_KIND_CONSTANT), emit
   `OP_ARRAY_SET_LONG (p=1 idx-const, q=value-is-const, A=dest, B=i, C=val)`
   — or narrow SET when everything fits 8 bits and the value is a register;
4. temps reset per element (each ei is its own expression).

nested literals recurse; the inner array is an element value like any other.
`[]` is NEW with count 0. a 300-element literal exercises the long encodings
and grow-under-collection — keep the generated/300 test.

### 5.4 for-in (special case, no iterator protocol)

`for let v in <expr> ?` desugars in the compiler to:

```
hidden local arr   = <expr>          // evaluated ONCE
hidden local len   = ARRAY_LEN arr   // cached
hidden local i     = 0
loop:
  i < len ? JUMP_IF exit             // compare + jump, polarity per house style
  v (loop scope) = ARRAY_GET arr, i
  <body>
  i += 1                              // ADD in place, the §5.2 table in action
  OP_LOOP loop
exit:
```

hidden locals live in the for's scope (begin_scope already gives us cleanup);
the loop var `v` is a normal binding inside that scope. when numeric-for
lands (tomorrow, per roadmap) its FOR_PREP/FOR_LOOP pair takes over the same
hidden-register pattern — write this desugar so the hidden-register
bookkeeping is a shared helper, not copy-paste.

parser discrimination: `for` followed by `let IDENT in` → for-in form;
anything else → existing C-style for (D14: frozen, not dropped). `for let v
in 5 ?` compiles the desugar and errors at runtime "can't iterate a number."
(no implicit ranges — ranges are numeric-for syntax, not values).

## 6. natives (D16 shape: natives-as-globals)

registered exactly like an embedder's natives — `ok_global_define_native_function`
— from ok_init (they must exist in the globals store BEFORE compile: name
resolution is compile-time, D2):

- `len(a)` → count as f64. non-array → loud error.
- `push(a, v)` → grow + append; returns null (no chaining cleverness);
  empty-capacity path follows array_grow_capacity. GC safety inside push:
  the array is rooted via the caller's argument window ([0, top) marking),
  and value_array_grow only reallocs the data buffer — the object_array
  struct never moves. no guard needed here, but say so in a comment so the
  next person doesn't "fix" it.
- `pop(a)` → tail or null on empty (locked). does NOT shrink capacity ever
  (fine; document).

embedders who build a raw vm without ok_init simply don't get them — same
as everything else; dorg3-style binders already re-register what they want.

## 7. GC checklist (the phase-2 memorial)

- [ ] `trace_object` gains `case OBJ_ARRAY: mark_value_array(p_gc,
      &((object_array*)p_object)->values); break;` — mark_value_array already
      exists (okgc.c:183, used for chunk constants).
- [ ] create_object_array allocation guarded end-to-end (§2).
- [ ] e2e closure-over-array test runs under aggressive+asan.
- [ ] remember: the switch has no default — a new OBJ_ value that falls
      through traces NOTHING. that silence is exactly the phase-2 bug. the
      test is the only thing that catches it.

## 8. tests (tests/e2e/tests/, runner picks up `// expect:` lines)

- arrays/: literal, empty, len, get/set roundtrip, nested, mixed
  value/reference elements, aliasing (`let b = a; b[0] = 1; a[0]`), literal
  evaluation order (side-effectful elements), for-in order + break/continue,
  for-in over closure-captured array.
- arrays/errors/: OOB get, OOB store, negative, 1.5, null index, string
  index, `==` on two distinct arrays (false — identity), iterate a number.
- arrays/regressions/: the make_counter dogfood (closures-as-objects with
  array state — the actual reason this feature exists), 300-element literal
  (long encodings + grow), grow-under-aggressive-GC soak via generated test,
  compound-assign single-evaluation (`a[f()] += g()` — f and g each called
  once; use counters).
- run everything under the 4-variant matrix (gcc / aggressive / asan /
  asan+aggressive) — scripts/run_e2e.sh + build_ok.sh as-is.

## 9. implementation order (one sitting, roughly)

1. okobject.h/.c: object_array + OBJ_ARRAY + create/deinit/dispatch.
2. okgc.c: trace case (§7).
3. okchunk.h: 8 opcode enum entries + contract comments.
4. okvm.c: resolve_array_index + 8 handlers (GET/SET/NEW/LEN × narrow/long).
5. okast.h/.c: uncomment/generate array + subscript + compound-assign nodes
   (bindings gain nothing; compound node holds {op, left, right}).
6. okparser.c: `[` prefix (literal) + infix (subscript) rules, compound
   assign table switch, for-in form.
7. okcompiler.c: lvalue union arm, resolve_lvalue case, store_lvalue case,
   subscript/literal/compound compilers, for-in desugar.
8. ok.c: len/push/pop registration.
9. tests §8, matrix green, THEN the disassembler printing pass for the new
   ops (okdebug.c) so v0.0.6 starts honest.

known risks, ranked: (a) the §5.1 checkpoint discipline (base/index temps
outliving the rhs compile) — the one genuinely new compiler invariant;
(b) allocation guard windows (§2) — pattern exists, copy it; (c) parser
for-form discrimination — mechanical. everything else is fill-in-the-blanks
against scaffolding that already exists.
