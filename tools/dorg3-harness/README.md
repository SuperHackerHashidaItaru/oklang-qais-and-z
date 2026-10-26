# dorg3 integration harness (no raylib needed)

standalone reproduction of dorg3's exact embed flow against any oklang build:
ok_create -> bind natives -> ok_run(dorg3.ok) -> fetch globals ->
ok_call(init/update/shutdown). built to reproduce and verify the
"invalid call: expected 8 arguments, got: 0" boot crash (2026-10-24 round).

## files
- repro.c        - the harness (C99, no deps beyond oklang itself)
- stubs.inc      - all 86 natives dorg3's binder registers, stubbed
                   (generated from runner/src/game/binder.cpp)
- values.inc     - the 23 binder value constants (colors/enums/keys), dummy 1s
- synthetic.ok   - minimal structural repro (glob fu init + fu draw(8 params))

## usage
```sh
gcc -std=c99 -I<oklang>/src -I<oklang>/include -I. \
    repro.c <oklang-build>/*.o -o repro
./repro /path/to/dorg3/game/assets/scripts/dorg3.ok
```

expected with the boot fix applied: ok_run OK, ok_call(init) OK (bool),
then update frames fail on stub data ("invalid operands" — stubs return
numbers where the game's real natives return textures/bools) WITH a full
file:line + stack trace. that failure is the harness proving the error
reporting works, not a VM bug.

run it under the aggressive and asan builds too — pre-fix, the aggressive
build reproduces the arity-8 crash deterministically.

## round 3 (2026-10-24 evening): "expected argument of type 'music'" — FIXED

repro2.c is the faithful harness for this round: all 86 binder natives with
real semantics — magic-tagged native values via ok_native_value_new
(TEXTURE/VECTOR2/FONT/SOUND/MUSIC/ARRAY wrappers), binder-identical type
checks and error messages, proper bool/number/string returns.

```sh
gcc -std=c99 -w -I<oklang>/src -I<oklang>/include -I. repro2.c \
    $(ls <oklang>/build-gcc/*.o | grep -v cli_main) -o repro2 -lm
./repro2 <dorg3>/game/assets/scripts/dorg3.ok
```

what it caught: stop_music(g_c_ballerina) passed, then stop_music(0x0) —
g_current_music held RAW 0x0 (the double 0.0, not tagged null). root cause
in the VM: OP_LOAD_NULL's operand B is a COUNT (okchunk.h: r[A..A+B-1])
but the interpreter looped `for (i = A; i < B; ++i)` — an exclusive end
index. any null materialized into a register other than r[0] wrote
nothing; the destination kept stale bits. `glob let mut g_current_music =
null` at dest r[1] therefore stored a stale 0.0 double into the global,
`g_current_music != null` compared number-vs-null and passed, and
stop_music(0.0) threw the type error. fixed in load-null-fix.patch
(+regression test tests/e2e/tests/regressions/load_null_count_operand.ok).

with the fix: ok_run OK, init OK, music_menu OK (stop_music + play_music
on real magic-tagged values), 3 update frames OK — on plain, aggressive
and asan builds. e2e 110/110 on all three.

note: repro2 also dumps music_menu's source_info/disassembly attempt —
music_menu is a plain `fu` (local closure, not a global), so the dump
prints "not an object"; that's expected. call it via update instead.

## round 4 (2026-10-25): "invalid call: value is not callable" + SIGSEGV — FIXED

repro3.c is the full-game soak harness for this round. upgrades over repro2:

- array natives are FAITHFUL: real ok_value vectors in ARRAY_MAGIC wrappers,
  binder-identical checks/messages, and array_foreach actually calls the
  script closure through ok_call exactly like binder.cpp does
- vector2 math is REAL (add/sub/mul/div/normalize/distance on wrapper x/y)
  so forks home in on the dorg and collisions actually fire
- random_range is a real inclusive int RNG (xorshift32, OK_SEED to reseed)
- input driver PLAYS THE GAME: clicks PLAY, steers the dorg with WASD in
  10s-dodge / 7s-stand-still cycles so forks catch it, clicks Retry on the
  death screen, repeats. heartbeats poll g_screen/g_dorg_dead/g_score/
  g_wave_state/g_dorg_pos/g_elapsed + per-array counts + a native census
  (clear_screen/create_color/is_key_down/array_length/random_range deltas)
  that pinpoints exactly where the update chain is at any frame.

```sh
gcc -std=c99 -w -I<oklang>/src -I<oklang>/include -I. repro3.c \
    $(ls <oklang>/build-gcc/*.o | grep -v cli_main) -o repro3_gcc -lm
OK_FRAMES=3600 ./repro3_gcc <dorg3>/game/assets/scripts/dorg3.ok
# expected: SOAK CLEAN, 4 deaths, waves restarting, exit 0
```

what it caught (byte-for-byte, both the error and the segfault, on plain
AND aggressive builds): the first native->VM callback in the project's
life — dorg3's array_foreach(forks, closure) at dorg3.ok:613 — crashed
three ways:

1. vm_call double-based the callee slot: call_value(reg, base, base, ...)
   passed the ABSOLUTE window base as the RELATIVE callee index, so the
   interpreter read data[base + base] instead of data[base]. every ok_call
   until now ran from C on an empty call stack (base == 0), where
   0 + 0 was accidentally correct.
2. when the callback errored, call_native returned success and the OP_CALL
   handler reloaded `frame` from a cleared call stack (data[count-1] with
   count == 0) — the segfault right after the error report.
3. (asan+aggressive) mark_vm walked stale register slots: call() raised top
   to frame_top without nulling the callee's not-yet-written window, so a
   collection mid-frame traced dangling pointers from dead frames.
4. (latent) call_native handed natives a pointer into the register file;
   any ok_call inside the native that grew the file left that pointer
   dangling for the rest of the native's execution (iteration 2+ of any
   callback loop).

fixed in native-callback-fixes.patch (okvm.c + okvm.h + embedding test
regression: native_apply calling back into the VM mid-script, verified to
segfault pre-fix and pass post-fix on plain/aggressive/asan/asan+aggressive).

one red herring worth remembering: the harness's first read of g_dorg_dead
used a numbers-only getter — bools are TAGGED values, so a dead dorg read
as 0 and the "frozen game" was actually the death screen patiently waiting
for a Retry click the driver never sent. the VM was fine; the driver was
blind. gbool() now reads tagged bools.

soak result with all fixes: 3600 frames (60s game time), 4912 array_foreach
calls, 3467 closure callbacks, 4 full death->Retry->restart cycles, waves
and arrays churning — identical output on gcc, aggressive, asan and
asan+aggressive. e2e 110/110 x3. embedding test OK x4.
