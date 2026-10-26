# Oklang Benchmark Results — Register VM, Lua, and Python

> Updated from the latest 30 workload reports supplied in the benchmark batch. Times are Hyperfine means and standard deviations in milliseconds. Ratios below are recomputed from the displayed means, rather than copied from each report’s “Relative” column.

## Executive summary

- **30 workloads** cover core interpreter operations, numeric loops, arrays, and scalar/array compound assignment.
- On the **8 workloads shared with the stack VM**, the register VM is faster in every case, with a **1.82x geometric-mean speedup** (range 1.06x–2.71x).
- Across all 30 workloads, Oklang’s register VM has a **1.14x geometric-mean runtime ratio versus Lua** (Oklang/Lua). A ratio below 1 means Oklang is faster; above 1 means Lua is faster. Oklang is faster in **5 of 30** workloads.
- Python is slower than Oklang in all 30 measured workloads. The **geometric-mean Python/Oklang ratio is 6.14x** (median 5.73x; range 2.63x–12.17x). These are interpreter-loop microbenchmarks, not a claim about Python workloads implemented in optimized native libraries.
- The strongest new assignment result is the **variable-index array pair**: `a[i] += 1` measures **9.6% less time** than `a[i] = a[i] + 1` in Oklang. The constant-index pair improves by about 4.0%, and the global-container pair by about 3.6%; interpret the latter two cautiously because the compound runs have more variation.

## How to read the ratios

- **Oklang/Lua** = Oklang mean ÷ Lua mean. `1.00x` is equal; below `1.00x` Oklang is faster; above `1.00x` Lua is faster.
- **Python/Oklang** = Python mean ÷ Oklang mean. `5.00x` means the Python run took five times as long.
- `mean ± sd` is the reported Hyperfine mean and standard deviation. Small differences should not be over-interpreted when they are similar to the run-to-run variation.

## Stack VM versus register VM

All eight shared baseline workloads improved after the register-VM rewrite.

| Workload | Stack VM (ms) | Register VM (ms) | Speedup | Lua (ms) |
|:--|--:|--:|--:|--:|
| `loop_counter` | 404.9 ± 6.6 | 197.2 ± 6.9 | 2.05× | 140.0 ± 1.3 |
| `arithmetic_sum` | 533.6 ± 10.7 | 233.6 ± 6.8 | 2.28× | 185.9 ± 6.5 |
| `branch_toggle` | 450.1 ± 6.5 | 181.5 ± 6.8 | 2.48× | 129.9 ± 0.9 |
| `local_pressure` | 181.0 ± 6.0 | 66.7 ± 1.5 | 2.71× | 59.1 ± 1.2 |
| `global_access` | 268.3 ± 7.0 | 136.0 ± 2.1 | 1.97× | 218.7 ± 4.2 |
| `function_calls` | 79.4 ± 1.1 | 55.8 ± 1.3 | 1.42× | 52.8 ± 1.7 |
| `closure_upvalue` | 47.1 ± 1.7 | 37.1 ± 4.8 | 1.27× | 31.6 ± 1.4 |
| `recursion_fib` | 20.2 ± 0.6 | 19.0 ± 1.0 | 1.06× | 15.1 ± 1.0 |

The largest improvements remain **local pressure (2.71×)**, **branch toggle (2.48×)**, and **arithmetic sum (2.28×)**. The smallest improvement is recursive Fibonacci (1.06×), where function-call/recursion costs dominate more of the short workload.

## Full results

### Core interpreter workloads

| Workload | Oklang register (ms) | Lua (ms) | Python (ms) | Oklang/Lua | Python/Oklang |
|:--|--:|--:|--:|--:|--:|
| `loop_counter` | 197.2 ± 6.9 | 140.0 ± 1.3 | 1122.8 ± 29.7 | 1.41× | 5.69× |
| `arithmetic_sum` | 233.6 ± 6.8 | 185.9 ± 6.5 | 2167.3 ± 50.4 | 1.26× | 9.28× |
| `branch_toggle` | 181.5 ± 6.8 | 129.9 ± 0.9 | 1376.8 ± 15.8 | 1.40× | 7.59× |
| `local_pressure` | 66.7 ± 1.5 | 59.1 ± 1.2 | 811.7 ± 15.2 | 1.13× | 12.17× |
| `global_access` | 136.0 ± 2.1 | 218.7 ± 4.2 | 544.7 ± 7.7 | 0.62× | 4.01× |
| `function_calls` | 55.8 ± 1.3 | 52.8 ± 1.7 | 292.3 ± 14.2 | 1.06× | 5.24× |
| `closure_upvalue` | 37.1 ± 4.8 | 31.6 ± 1.4 | 158.5 ± 6.0 | 1.17× | 4.27× |
| `recursion_fib` | 19.0 ± 1.0 | 15.1 ± 1.0 | 49.9 ± 2.9 | 1.26× | 2.63× |

### Numeric `for` workloads

| Workload | Oklang register (ms) | Lua (ms) | Python (ms) | Oklang/Lua | Python/Oklang |
|:--|--:|--:|--:|--:|--:|
| `numeric_for_counter` | 146.3 ± 1.7 | 76.1 ± 1.9 | 1559.4 ± 15.3 | 1.92× | 10.66× |
| `numeric_for_sum` | 148.2 ± 6.1 | 75.6 ± 0.8 | 1633.8 ± 14.9 | 1.96× | 11.02× |
| `numeric_for_step` | 74.9 ± 1.1 | 39.8 ± 0.2 | 838.0 ± 15.8 | 1.88× | 11.19× |

Numeric loops remain a clear gap: Oklang is **1.88–1.96× Lua’s time** on these numeric-for cases. For the summation workload, changing from the general-loop baseline (`arithmetic_sum`) to `numeric_for_sum` reduces runtime by approximately **36.6% in Oklang**, **59.3% in Lua**, and **24.6% in Python**. Lua’s specialized numeric loop benefits more in this test.

### Array workloads

| Workload | Oklang register (ms) | Lua (ms) | Python (ms) | Oklang/Lua | Python/Oklang |
|:--|--:|--:|--:|--:|--:|
| `array_index_sum` | 346.9 ± 13.8 | 274.9 ± 5.6 | 1998.1 ± 9.3 | 1.26× | 5.76× |
| `array_for_in_sum` | 270.3 ± 9.9 | 493.1 ± 6.8 | 1329.1 ± 12.9 | 0.55× | 4.92× |
| `array_for_in_sum_indexed` | 267.6 ± 6.1 | 218.2 ± 8.7 | 1347.4 ± 32.6 | 1.23× | 5.04× |
| `array_push_growth` | 17.8 ± 1.2 | 31.4 ± 2.5 | 78.5 ± 2.8 | 0.57× | 4.41× |
| `array_literal_alloc` | 52.8 ± 3.3 | 47.3 ± 2.6 | 148.6 ± 8.6 | 1.12× | 2.81× |
| `array_subscript_add_plain` | 97.3 ± 1.6 | 87.8 ± 1.4 | 413.1 ± 4.8 | 1.11× | 4.25× |
| `array_subscript_add_compound` | 93.4 ± 5.7 | 87.4 ± 3.1 | 387.9 ± 8.4 | 1.07× | 4.15× |
| `array_subscript_variable_add_plain` | 105.6 ± 1.3 | 89.1 ± 6.5 | 540.8 ± 18.6 | 1.19× | 5.12× |
| `array_subscript_variable_add_compound` | 95.4 ± 1.0 | 88.0 ± 2.4 | 423.7 ± 8.3 | 1.08× | 4.44× |
| `global_array_subscript_add_plain` | 95.3 ± 1.7 | 138.0 ± 5.8 | 403.0 ± 10.4 | 0.69× | 4.23× |
| `global_array_subscript_add_compound` | 91.8 ± 2.9 | 136.4 ± 2.7 | 382.0 ± 10.8 | 0.67× | 4.16× |

#### `for-in` versus Lua indexing

| Variant | Oklang (ms) | Lua (ms) | Python (ms) | Interpretation |
|:--|--:|--:|--:|:--|
| `array_for_in_sum` (Lua uses `ipairs`) | 270.3 ± 9.9 | 493.1 ± 6.8 | 1329.1 ± 12.9 | Oklang is 1.82× faster than the Lua `ipairs` variant. |
| `array_for_in_sum_indexed` (Lua numeric index loop) | 267.6 ± 6.1 | 218.2 ± 8.7 | 1347.4 ± 32.6 | Lua is 1.23× faster than Oklang in this comparison. |

The Oklang times are effectively unchanged across the two cases (about 1.0% difference), while the Lua time drops by **55.7%** when moving from `ipairs` to a numeric index loop. This is why the indexed comparator is important: the first result measures loop/iterator idiom differences as well as array access. Python is also close across the two cases, with the indexed report about 1.4% slower.

#### New indexed compound-assignment pairs

| Oklang operation | Plain assignment (ms) | Compound assignment (ms) | Compound change |
|:--|--:|--:|--:|
| Constant index: `a[0] = a[0] + 1` vs `a[0] += 1` | 97.3 ± 1.6 | 93.4 ± 5.7 | **4.0% less time** |
| Variable index: `a[i] = a[i] + 1` vs `a[i] += 1` | 105.6 ± 1.3 | 95.4 ± 1.0 | **9.6% less time** |
| Global container: `g[0] = g[0] + 1` vs `g[0] += 1` | 95.3 ± 1.7 | 91.8 ± 2.9 | **3.7% less time** |

The variable-index result is the clearest of these three: the compound form is consistently faster in the reported timings, despite using a register-index operand rather than the constant-index path. The constant-index and global-container gains are smaller and their compound timing spread is wider, so a repeat paired run would make the conclusion stronger. These measurements are consistent with less repeated target/index handling in compound assignment, but timing alone does not establish the exact bytecode or runtime mechanism.

### Scalar compound assignments

| Operation | Plain (ms) | Compound (ms) | Compound change in Oklang |
|:--|--:|--:|--:|
| Addition | 118.0 ± 1.5 | 118.9 ± 7.1 | +0.77% time |
| Subtraction | 117.9 ± 2.7 | 117.7 ± 1.7 | -0.10% time |
| Multiplication | 50.6 ± 1.6 | 51.1 ± 2.9 | +0.85% time |
| Division | 53.0 ± 2.1 | 52.9 ± 0.9 | -0.25% time |

Scalar compound assignment remains a wash: the deltas are below 1%, smaller than the run-to-run noise, and do not show a measurable general slowdown. The Lua “compound” files use Lua-compatible ordinary assignment because standard Lua syntax does not provide `+=`; therefore those rows are not a Lua compound-syntax comparison.

## Highlights

### Where Oklang is faster than Lua

| Workload | Oklang (ms) | Lua (ms) | Relative result |
|:--|--:|--:|:--|
| `array_for_in_sum` | 270.3 | 493.1 | Oklang is 1.82× faster |
| `array_push_growth` | 17.8 | 31.4 | Oklang is 1.76× faster |
| `global_access` | 136.0 | 218.7 | Oklang is 1.61× faster |
| `global_array_subscript_add_compound` | 91.8 | 136.4 | Oklang is 1.49× faster |
| `global_array_subscript_add_plain` | 95.3 | 138.0 | Oklang is 1.45× faster |

These wins are concentrated in array growth, `for-in` versus `ipairs`, and global access/container cases. Oklang global identifiers are resolved to indexed slots at compile time, so the global benchmarks are particularly relevant to that implementation choice. The new global-array pair is faster than Lua in both forms in this data set, but should be viewed as a microbenchmark rather than a universal claim about global access.

### Where Lua has the largest lead

| Workload | Oklang/Lua | Summary |
|:--|--:|:--|
| `numeric_for_sum` | 1.96× | Lua is 1.96× faster by mean runtime ratio |
| `numeric_for_counter` | 1.92× | Lua is 1.92× faster by mean runtime ratio |
| `numeric_for_step` | 1.88× | Lua is 1.88× faster by mean runtime ratio |
| `loop_counter` | 1.41× | Lua is 1.41× faster by mean runtime ratio |
| `branch_toggle` | 1.40× | Lua is 1.40× faster by mean runtime ratio |

The largest gaps are the specialized numeric-for workloads, then the general loop and branch workloads. They suggest loop/control-flow dispatch is still a productive area to profile.

## Measurement notes and limitations

- All values are milliseconds from the latest reports pasted in the conversation; each report displays mean, standard deviation, minimum, and maximum. The report excerpts show 10-run JSON samples for a subset of benchmarks.
- The supplied JSON excerpts show exit code `0` for all commands in those excerpts. The Markdown-only tables do not include exit-code fields, and the pasted material does not provide stdout for independent output-equivalence verification.
- Cross-language comparisons are only as fair as the paired source programs. Lua uses 1-based arrays, Oklang/Python use 0-based indexing; benchmark scripts target corresponding elements. Some operations, notably `push` and Lua `ipairs`, use idiomatic per-language constructs with different overheads.
- Memory values in Hyperfine JSON appear to be process-level measurements. The pasted data does not document the measurement method, so they are not treated here as language-level allocation totals.
- Run-to-run standard deviation is not a confidence interval. Very small timing changes should not be treated as established speedups without paired, repeated runs or profiling.

## Suggested next steps

1. Keep the variable-index pair: it exercises the register-operand form and currently shows the strongest compound-assignment gain.
2. Repeat plain/compound array pairs in interleaved order to reduce drift and confirm the ~9.7% variable-index difference.
3. Inspect/disassemble `a[i] = a[i] + 1` and `a[i] += 1` to verify which loads/stores or target calculations are removed.
4. Use the indexed Lua `for` loop, not `ipairs`, as the primary apples-to-apples comparison for array iteration; keep `ipairs` as a separate idiomatic comparison.
5. Profile numeric-for and branch-heavy workloads before optimizing based only on these timings.
