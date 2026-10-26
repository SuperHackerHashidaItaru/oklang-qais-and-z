# Compound vs plain assignment — bytecode evidence

Settles the `a[0] = a[0] + 1` vs `a[0] += 1` question from the round-11 follow-up.
Produced with `ok --dump` on `ok-reg-fresh` (the exact binary benched in `results/`), 2026-10-26.

## Verdict

**The bench was right, and it was read backwards.** Compound subscript assignment is
**faster**, not slower:

- user's machine (hyperfine, 10 runs): plain 96.5 ± 1.7 ms, compound 92.7 ± 1.6 ms → compound ~3.96% faster
- second machine (5 runs, direct): plain 91.0–91.4 ms, compound 84.4–84.8 ms → compound ~7% faster

Scalars: `x = x + 1` and `x += 1` compile to **identical bytecode** — one `ADD r1 = r1 + k`.
"No measurable difference" is the best possible result: since round 11 the two paths converge.

## Why (disassembly, loop bodies)

`a[0] = a[0] + 1` — 5 dispatches per iteration for the statement:

```
0009  MOVE      r5 = r1           ; stage container for the SET (lvalue keep-alive)
0010  MOVE      r7 = r1           ; stage container for the GET (rvalue's own subscript)
0011  ARRAY_GET r4 = r7[k7 '0']   ; read a[0]
0012  ADD       r4 = r4 + k8 '1'  ; (round-11 peephole: no temp+MOVE)
0013  ARRAY_SET r5[k6 '0'] = r4   ; write a[0]
```

`a[0] += 1` — 4 dispatches:

```
0009  MOVE      r5 = r1           ; single container resolution
0010  ARRAY_GET r7 = r5[k6 '0']
0011  ADD       r7 = r7 + k7 '1'
0012  ARRAY_SET r5[k6 '0'] = r7
```

The plain source contains **two subscript expressions** (one in the rvalue, one as the
lvalue), each materializing the container into its own register window. The compound form
contains **one**. That is the single-evaluation semantics (`a[f()] += 1` calls `f` once)
paying a measurable dividend, not a compiler inefficiency in either path.

The arithmetic checks out: ~3.8 ms over 2M iterations ≈ 1.9 ns/iter ≈ one cheap dispatch —
exactly the eliminated `MOVE` plus one fewer register write.

## Resolution of the session-8 "+= accounting" question

- scalars: `+=` is sugar — identical bytecode since the round-11 assign/binop peephole.
- subscripts: `+=` is genuinely better — one container resolution instead of two.

## Parked follow-up (NOT a compound-assign issue)

Both forms still pay one staging `MOVE` when the container is already a simple local.
A container-elision peephole (skip materialization when the container is a local provably
not reseated by the index/RHS evaluation — same guard family as
`regressions/assign_operand_read_timing.ok`) would take both forms to 3 dispatches.
That is the parked "MOVE staging" item; it optimizes *below* the convention, it does not
remove it. Candidate for post-jam, or buffer-day if array-heavy game loops show it hot.
