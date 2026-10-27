# patches — pending

deliverables awaiting Q's review + hand-application, newest last. apply in
order (each patch is stacked on the previous):

1. `05-modulo.patch` — vs `4633dc9` (master, your buffer-night squash).
   `%` / `%=`: OP_MODULUS(+_LONG), trunc/fmod semantics (sign follows the
   dividend — decision to ratify, see guide), the TOKEN_MODULO_EQUAL pratt
   fix, `modulo by zero.` error. **requires `target_link_libraries(ok m)`**
   (fmod). e2e 203/203.
2. `06-forloop-imm-fastpath.patch` — stacked on 05. the numeric-for gap
   experiment: FOR_PREP normalizes the counter/limit/step triple, FOR_LOOP's
   p flag carries the compiler's immutability proof — the hot back edge reads
   raw doubles, no per-trip tag checks. `let mut` keeps the gate. sandbox:
   ~0-5% (honest), codegen verified tag-check-free; your rig arbitrates.
   revert `3789e48` alone if it's flat there too — modulo survives.

`REVIEW-GUIDE-night13.md` — the whole-night bulk-review guide: per-patch
where-to-look, the proof obligations for 06, the 5-minute re-checks for the
already-yolo'd 04, the -lm build note, open items (v0.0.5 tag still unpushed).

after Q applies + pushes, these move to archive/patches/ and this dir empties.

## history

- rounds 10's arrays-v2 + default-args patches: APPLIED by Q (squash
  `9e9fd41`), archived under archive/patches/.
- rounds 11's numeric-for / disassembler / assign-peephole (01-03): APPLIED
  by Q (squash `55e1293`).
- round 12.5's buffer-night patch (04: ternary, string concat, equality fix,
  token purge, elision, window fix): APPLIED by Q (squash `4633dc9`, yolo).
