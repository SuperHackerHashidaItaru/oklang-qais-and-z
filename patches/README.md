# patches — pending

deliverables awaiting Q's review + hand-application, newest last. apply in
order (default-args is stacked on arrays):

1. `arrays-v2.patch` — vs `cdefd61` (v0.0.4 + the ok.c strict fix). arrays:
   literals, subscript lvalues, compound assign, for-in (`:`), natives
   len/push/pop/remove. e2e 146/146 x4 variants + dorg3 harness soak clean x4.
2. `default-args.patch` — vs the arrays commit (`63be373`). stacked: apply
   after arrays-v2. call-time defaults, trailing-only, OP_JUMP_IF_ARGC ladder.
   e2e 160/160 x4 variants + harness clean.

after Q applies + pushes, these move to archive/patches/ and this dir empties.
