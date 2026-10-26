# Oklang / Lua / Python benchmark suite (v2)

This suite compares the register-based Oklang VM to Lua and Python, and optionally compares the older stack VM on the legacy subset. It uses the Oklang language features established by the existing tests: numeric `for`, arrays, indexed reads/writes, `for-in`, `push`, and compound assignment. It does not use game-specific C API functions.

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
| `array_for_in_sum` | Array iteration / for-in traversal; 8 million visited elements. | 1,000,000 passes (8 million elements) | `36000000` |
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
| `array_subscript_add_plain` | Array-element add: plain assignment. Lua uses plain assignment for both paired cases. | 2,000,000 | `2000000` |
| `array_subscript_add_compound` | Array-element add: compound assignment. Lua uses plain assignment for both paired cases. | 2,000,000 | `2000000` |

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

The same pair exists for a fixed array element: `a[0] = a[0] + 1` vs `a[0] += 1`. Lua has no compound-assignment syntax, so its paired benchmark files intentionally execute the equivalent plain-assignment expression in both cases. The Lua timing is a control, not a compound-assignment implementation.

The multiplication/division benchmarks use a factor of `1.0000001` over two million iterations to avoid immediately collapsing to zero or overflowing. Treat printed float text as approximate; the resulting value should be very close across runtimes.

## How to read the numbers fairly

- Run release builds with the same C compiler options for the two Oklang VMs. Disable debug disassembly/profiling.
- All commands include process startup and Oklang parsing/compilation. Long loops emphasize runtime execution; array-literal allocation includes actual per-iteration array allocations.
- `numeric_for_*` deliberately compares Oklang's exclusive limit with Python's `range` and Lua's inclusive numeric-for endpoint adjusted to execute the same number of iterations.
- The while-loop baselines are retained so you can compare generic looping against numeric-for specialization.
- `array_index_sum` uses explicit indexing. `array_for_in_sum` uses Oklang's `for-in`, Lua's `ipairs`, and Python's native list iteration; those are idiomatic counterparts, not identical implementation strategies.
- Oklang uses `%g` formatting in the register VM while an older build may use `%f`. Output formatting can differ; validate numeric results, not exact text, and the single final print has negligible influence on these loop timings.
- The static indexed-global test measures Oklang's compile-time-resolved global slots, not dynamic name lookup. Do not interpret it as a generic comparison of global-name resolution.
- Make sure every benchmark finishes successfully and produces the documented result before trusting timings. The runner checks command exit status through Hyperfine but does not automatically compare output values.

## Validation checklist

Before comparing results, run each `.ok` file once with `./ok` and verify the output. In particular, check numeric-for step semantics, array `push`/indexing, `for-in`, and each compound operator. Run with 10 measured runs for exploration and 20+ for final comparisons.
