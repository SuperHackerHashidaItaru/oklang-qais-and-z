# patches — pending

deliverables awaiting Q's review + hand-application, newest last. apply in
order (each patch is stacked on the previous):

1. `01-numeric-for.patch` — vs `9e9fd41` (master after arrays + defaults).
   numeric for (`for let (mut)? i = a : b (: c)? ? stmt`), OP_FOR_PREP /
   OP_FOR_LOOP, for-in converges on the same back edge. e2e 182/182.
2. `02-disassembler.patch` — vs the numeric-for commit. per-opcode
   disassembler, `ok --dump`, ok_debug_dump_bytecode hook.
3. `03-assign-binop-direct.patch` — vs the disassembler commit. the
   local_pressure fix: simple-left binop assignments compile straight into
   the local's register. e2e 185/185.

`REVIEW-GUIDE-round11.md` — the where-to-look guide for all three (what is
load-bearing vs noise, benchmark table, the refuted-theory writeup).

after Q applies + pushes, these move to archive/patches/ and this dir empties.

## history

- rounds 10's arrays-v2 + default-args patches: APPLIED by Q (squash
  `9e9fd41`), archived under archive/patches/.
