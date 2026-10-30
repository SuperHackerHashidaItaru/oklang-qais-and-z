# spec: arrays (target: 25/10)

goal: contiguous, GC-tracked value arrays. enough for the game to fake
structs (`let self = [hp, x, y];`) and for varargs to build on later.

## surface

```ok
let a = [1, 2, 3];              // literal, empty: []
let mut m = [0, 0];
m[0] = 5;                       // index store (index must be int-ish)
print a[1];                     // index load
push(m, 42);                    // append, grows capacity x2 from 8
let last = pop(m);              // remove + return tail (null on empty)
print len(m);                   // count
for let v in m ?                // iteration (special-cased, see below)
  print v;
```

- indexing is number-only, 0-based. float with integral value accepted
  (matches the int/number duality in perform_*). OOB load = runtime error
  "index out of bounds: 5 (len 3)" — loud, with file:line like everything
  else now. OOB store same.
- `==` on arrays: identity (pointer), like everything non-string today.
  equality-by-content is a classes-era decision.

## representation

```c
typedef struct {
  object object;
  uint32_t count;
  uint32_t capacity;      // power-of-two growth from 8, like the register file
  value* data;            // single alloc, realloc'd; GC-traced
} object_array;
```

- new object type OBJ_ARRAY in the object enum, deinit releases data.
- `trace_object` gains `case OBJ_ARRAY: mark_value_array(...)`.
  **this is the bug class from this round — do not forget the trace case.**
- allocation of the struct and the data buffer are two allocs: create the
  struct, immediately attach data, only then publish. with the current gc
  (non-moving, guard slots) the safe order is: alloc data (no ok values in
  it yet — fill with nulls), then the object wrapper referencing it, then
  write into a register. under OK_AGGRESSIVE_GC the array must be in a
  rooted register before the first `push` that can realloc.

## opcodes (register-machine shaped, no stack juggling)

| op | operands | semantics |
|----|----------|-----------|
| OP_ARRAY_NEW | a'dest, b'len_const_or_reg(p) | r[a] = new array, null-filled; count = p ? k[b] : r[b] |
| OP_ARRAY_GET | p,a',b',c | r[a] = r[b][ resolve(c,p) ] |
| OP_ARRAY_SET | p,a',b',c | r[a][ resolve(b,p) ] = r[c] — note dest order matches OP_MOVE family |
| OP_ARRAY_LEN | a',b' | r[a] = len r[b] |

push/pop stay natives initially (they can error nicely and they don't need
opcode budget); revisit as ops if the profiler says so. bounds checks in the
ops are one compare each — cheaper than a call.

## iteration

`for let v in <expr>` — v1 special cases (no generic iterator protocol yet):
- expr is array -> elements in order
- expr is number -> ERROR "can't iterate a number" (no implicit range;
  ranges come with numeric-for syntax, not values)

implementation: desugar in the compiler to an index local (hidden), a len
local (cached — len doesn't re-read per iter), ARRAY_GET + JUMP_IF-style
loop. after numeric-for lands, reuse its hidden-register machinery so both
loops share the tight-exit shape.

## tests (add to e2e before merging)
literal/len/get/set/push/pop roundtrip, empty-array pop -> null, OOB get+store
(expect error), nested arrays, array captured by closure (upvalue keeps it
alive across gc — run under OK_AGGRESSIVE_GC), for-in order, mutation during
iteration is allowed and reads the mutated array (document it), 300-element
array (exercises growth + the 257+ constant/register long encodings).
