# Oklang / Lua / Python benchmark suite (v3)

This suite compares the register-based Oklang VM to Lua and Python, and optionally compares the older stack VM on the legacy subset. Version 3 adds the variable-index and global-container array-assignment pairs plus a Lua numeric-index-loop comparator for `for-in`. It uses the Oklang language features established by the existing tests: numeric `for`, arrays, indexed reads/writes, `for-in`, `push`, and compound assignment. It does not use game-specific C API functions.

## What is included

### Existing / stack-compatible baseline

| Benchmark | Workload | Iterations / calls | Expected output |
|---|---|---:|---:|
| `loop_counter` | while-loop local increment | 10,000,000 | `10000000` |
| `arithmetic_sum` | while loop + arithmetic accumulator | 10,000,000 | `49999995000000` |
| `branch_toggle` | alternating conditional branches | 5,000,000 | `5000000` |
| `local_pressure` | eight live scalar locals | 1,000,000 | `36000000` |
| `global_access` | mutable global read/write | 5,000,000 | `5000000` |
| `function_calls` | one million ordinary calls | 1,000,000 calls | `1000000` |
| `closure_upvalue` | returned closure updating captured local | 500,000 calls | `500000` |
| `recursion_fib` | naive recursive Fibonacci | fib(25) | `75025` |

### New-feature workloads (register VM only)

The old stack VM is deliberately skipped for these because it does not implement these syntax/features.

| Benchmark | Workload | Iterations | Expected output |
|---|---|---:|---:|
| `numeric_for_counter` | Numeric for loop mechanics with unit step; compare with the existing while-loop counter. | 10,000,000 | `10000000` |
| `numeric_for_sum` | Numeric for with accumulator; compare against arithmetic_sum which uses while. | 10,000,000 | `49999995000000` |
| `numeric_for_step` | Numeric for with non-unit step; 5 million iterations. | 5,000,000 | `24999995000000` |
| `array_index_sum` | Repeated numeric indexing and reads from a prebuilt array/list. | 1,000,000 passes (8 million elements) | `36000000` |
| `array_for_in_sum` | Array iteration / for-in traversal; Lua uses `ipairs`. | 1,000,000 passes (8 million elements) | `36000000` |
| `array_for_in_sum_indexed` | Oklang `for-in` vs Lua numeric index loop vs Python list iteration; 8 million visited elements. | 1,000,000 passes (8 million elements) | `36000000` |
| `array_push_growth` | Array allocation and dynamic growth through repeated append/push. | 200,000 | `399999` |
| `array_literal_alloc` | Array literal construction/allocation inside a loop plus element reads. | 200,000 | `40000400000` |
| `assign_add_plain` | + assignment baseline. Iterations: 5,000,000. Lua uses its equivalent plain assignment because Lua has no compound assignment syntax. | 5,000,000 | `5000000` |
| `assign_add_compound` | += compound assignment; compare Oklang and Python against the plain form. Iterations: 5,000,000. Lua uses its equivalent plain assignment because Lua has no compound assignment syntax. | 5,000,000 | `5000000` |
| `assign_sub_plain` | - assignment baseline. Iterations: 5,000,000. Lua uses its equivalent plain assignment because Lua has no compound assignment syntax. | 5,000,000 | `0` |
| `assign_sub_compound` | -= compound assignment; compare Oklang and Python against the plain form. Iterations: 5,000,000. Lua uses its equivalent plain assignment because Lua has no compound assignment syntax. | 5,000,000 | `0` |
| `assign_mul_plain` | * assignment baseline. Iterations: 2,000,000. Lua uses its equivalent plain assignment because Lua has no compound assignment syntax. | 2,000,000 | `~1.2214` |
| `assign_mul_compound` | *= compound assignment; compare Oklang and Python against the plain form. Iterations: 2,000,000. Lua uses its equivalent plain assignment because Lua has no compound assignment syntax. | 2,000,000 | `~1.2214` |
| `assign_div_plain` | / assignment baseline. Iterations: 2,000,000. Lua uses its equivalent plain assignment because Lua has no compound assignment syntax. | 2,000,000 | `~0.81873` |
| `assign_div_compound` | /= compound assignment; compare Oklang and Python against the plain form. Iterations: 2,000,000. Lua uses its equivalent plain assignment because Lua has no compound assignment syntax. | 2,000,000 | `~0.81873` |
| `array_subscript_add_plain` | Array-element add with literal index `0`: plain assignment. Lua uses plain assignment for both paired cases. | 2,000,000 | `2000000` |
| `array_subscript_add_compound` | Array-element add with literal index `0`; compound assignment. Lua uses plain assignment for both paired cases. | 2,000,000 | `2000000` |
| `array_subscript_variable_add_plain` | Array-element add with local variable index `i`; register-index operand path. | 2,000,000 | `2000000` |
| `array_subscript_variable_add_compound` | Same variable-index case using `a[i] += 1`. | 2,000,000 | `2000000` |
| `global_array_subscript_add_plain` | Array-element add through global container `g`, using literal index `0`. | 2,000,000 | `2000000` |
| `global_array_subscript_add_compound` | Same global-container case using `g[0] += 1`. | 2,000,000 | `2000000` |
| `map_insert` | Insert-heavy maps: 200k string-key inserts into an empty map, forcing repeated rehash growth. Lua uses table assignment, Python dict assignment. | 200,000 | `199999` |
| `map_lookup` | Lookup-heavy maps: 8 million string-key reads over a prebuilt 8-entry map; the map sibling of `array_index_sum`. | 1,000,000 passes (8M reads) | `36000000` |
| `map_churn` | Churn-heavy maps: interleaved insert/remove (odd keys survive), exercising backward-shift deletion and slot reuse. | 200,000 | `199999` |
| `map_for_in_sum` | Map iteration: Oklang `for let k, v :`, Lua `pairs`, Python key iteration + value lookup; 8M visited entries. | 1,000,000 passes (8M visited) | `36000000` |
| `mr_call` | Multi-result calls: two-value return destructured per trip (`let (x, y) = pair(...)`), vs Lua multi-return and Python tuple-return + unpacking. | 1,000,000 | `1000000` |
| `varargs_pack` | Varargs packing: `sum_all(i, i+1, i+2)` per trip (one pack allocation per call), vs Lua `...` (also packs a table) and Python `*args`. | 200,000 | `60000300000` |

## Run it

Requirements: `hyperfine`, `lua`, and `python3`. Run from the directory containing your Oklang `ok` executable. Extract this `oklang-bench-suite` directory somewhere under or next to your build directory.

```bash
cd /path/to/oklang/build
OK_REGISTER="$PWD/ok" RUNS=10 WARMUP=2 ../oklang-bench-suite/run-benchmarks.sh
```

If you also retain the old stack VM binary, the legacy benchmarks can compare it:

```bash
OK_REGISTER="$PWD/ok" OK_STACK="$PWD/ok-stack" RUNS=10 WARMUP=2 ../oklang-bench-suite/run-benchmarks.sh
```

If the suite directory is inside the current build directory, use `./oklang-bench-suite/run-benchmarks.sh` instead. The runner stores each Hyperfine JSON and Markdown report in `results/`.

To run more repetitions:

```bash
RUNS=20 WARMUP=3 ./oklang-bench-suite/run-benchmarks.sh
```

## The compound-assignment comparisons

For each operator, there are separate Oklang/Python benchmarks for the plain form and compound form:

- `x = x + 1` vs `x += 1`
- `x = x - 1` vs `x -= 1`
- `x = x * factor` vs `x *= factor`
- `x = x / factor` vs `x /= factor`

Array-subscript pairs now cover three paths: a literal local-array index (`a[0]`), a local-variable index (`a[i]`, intended to exercise register-index operands), and a literal index into a global array (`g[0]`, exercising the global-container load path). The variable index is held fixed at runtime to avoid adding index-cycling overhead; the source uses a local identifier rather than a literal index. Lua has no compound-assignment syntax, so its paired benchmark files intentionally execute the equivalent plain-assignment expression in both cases. The Lua timing is a control, not a compound-assignment implementation.

The multiplication/division benchmarks use a factor of `1.0000001` over two million iterations to avoid immediately collapsing to zero or overflowing. Treat printed float text as approximate; the resulting value should be very close across runtimes.

## How to read the numbers fairly

- Run release builds with the same C compiler options for the two Oklang VMs. Disable debug disassembly/profiling.
- All commands include process startup and Oklang parsing/compilation. Long loops emphasize runtime execution; array-literal allocation includes actual per-iteration array allocations.
- `numeric_for_*` deliberately compares Oklang's exclusive limit with Python's `range` and Lua's inclusive numeric-for endpoint adjusted to execute the same number of iterations.
- The while-loop baselines are retained so you can compare generic looping against numeric-for specialization.
- `array_index_sum` uses explicit indexing. `array_for_in_sum` uses Oklang's `for-in`, Lua's `ipairs`, and Python's native list iteration; those are idiomatic counterparts, not identical implementation strategies.
- Oklang uses `%g` formatting in the register VM while an older build may use `%f`. Output formatting can differ; validate numeric results, not exact text, and the single final print has negligible influence on these loop timings.
- `map_for_in_sum` compares Oklang's two-binding form (k and v delivered by the loop op) against Lua's `pairs` (also k + v) and Python's `for key in m: m[key]` - the idiomatic way to get both key and value per trip in each language. Python's `.values()` would only be a fair comparison for a keys-discarding variant.
- `mr_call` is the spans milestone's honesty check: the destructuring `let` binds the call's result window in place (zero MOVEs), so Oklang should hold its own against Lua's stack-slot multi-return and beat Python's tuple allocation. Sandbox preview (-O2): Oklang ~82ms vs Python ~345ms (4.2x); the Lua side needs the CI runner.
- `varargs_pack` allocates one packed array per call, same as Lua's `{...}` table and Python's `*args` tuple - an honest comparison of the always-pack v1 semantics. Sandbox preview: Oklang ~47ms vs Python ~94ms (2x).
- `array_for_in_sum` timings across Oklang versions: since the maps patch, the ranged for-in is lowered onto the ITER op trio (no more `len` call at loop entry). On the sandbox preview the array for-in benchmarks improved ~11-12% from this; the numbers below were taken with the new lowering.
- The static indexed-global test measures Oklang's compile-time-resolved global slots, not dynamic name lookup. Do not interpret it as a generic comparison of global-name resolution.
- Make sure every benchmark finishes successfully and produces the documented result before trusting timings. The runner checks command exit status through Hyperfine but does not automatically compare output values.

## Validation checklist

Before comparing results, run each `.ok` file once with `./ok` and verify the output. In particular, check numeric-for step semantics, array `push`/indexing, `for-in`, and each compound operator. Run with 10 measured runs for exploration and 20+ for final comparisons.
