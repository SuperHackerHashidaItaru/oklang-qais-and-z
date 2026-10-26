# Oklang benchmark results — analysis and readout

## Executive summary

**The register VM is about 1.85× faster than the old stack VM geometrically across the eight workloads both versions support.** The largest relative improvements are local-variable pressure (~2.67×), branch toggle (~2.52×), arithmetic sum (~2.27×), and loop counter (~2.08×). Function calls (~1.47×), closures/upvalues (~1.32×), and especially recursion (~1.16×) improve less, which suggests those workloads have significant costs beyond operand-stack traffic.

Against Lua, Oklang is close on function calls, closures, local pressure, and recursive Fibonacci; it is faster on the static global-access benchmark and on the two array cases involving `for-in` traversal and growth. Lua remains faster on numeric loops, branch-heavy loops, arithmetic, and indexed array reads. Oklang is faster than Python in all 25 measured workloads in this result set, by roughly 2.7× to 12.4×; treat that as a description of these specific scripts, not a universal language ranking.

**Compound assignment did not show a measurable slowdown in these runs.** For scalar `+=`, `-=`, `*=`, `/=`, Oklang compound and ordinary assignment timings are nearly the same; array-subscript `+=` was about 4% faster than the plain read/add/store version.

## How to read these numbers

- Times are **mean ± standard deviation**, in milliseconds, taken from the uploaded combined JSON reports. Each command has 10 recorded measurements. `±` is run-to-run spread, not a confidence interval.
- All recorded process exit codes are zero. The report does not capture or compare stdout, so it confirms successful process exits, **not** that all printed numeric results were identical. Verify expected outputs separately when changing the suite.
- Timings include process startup and the language runtime loading/compiling the source. The longer-loop tests emphasize execution, but small allocations and call benchmarks are more affected by startup and runtime initialization.
- `Oklang/Lua` below is the ratio of mean runtimes: below 1.00 means Oklang took less time; above 1.00 means Lua took less time. `Python/Oklang` above 1.00 means Python took longer.

## 1. Existing VM baseline: stack vs register

| Workload | Stack VM (ms) | Register VM (ms) | Stack → register speedup | Lua (ms) | Python (ms) | Oklang/Lua time | Python/Oklang time |
|---|---:|---:|---:|---:|---:|---:|---:|
| Loop counter | 403.9 ± 4.9 | 194.3 ± 3.5 | **2.08×** | 139.1 ± 1.8 | 1110.7 ± 14.8 | 1.40× | 5.72× |
| Arithmetic sum | 534.1 ± 8.7 | 235.2 ± 3.0 | **2.27×** | 185.8 ± 1.9 | 2206.4 ± 67.6 | 1.27× | 9.38× |
| Branch toggle | 451.1 ± 7.1 | 179.2 ± 4.0 | **2.52×** | 133.0 ± 7.7 | 1388.8 ± 22.3 | 1.35× | 7.75× |
| Local-variable pressure | 179.4 ± 2.6 | 67.2 ± 1.4 | **2.67×** | 59.5 ± 1.9 | 830.7 ± 27.3 | 1.13× | 12.37× |
| Global access | 265.0 ± 1.5 | 137.9 ± 7.8 | **1.92×** | 217.8 ± 4.8 | 551.5 ± 12.6 | 0.63× | 4.00× |
| Function calls | 81.5 ± 3.5 | 55.5 ± 1.4 | **1.47×** | 51.1 ± 8.9 | 293.2 ± 12.2 | 1.08× | 5.29× |
| Closure/upvalue | 47.4 ± 1.7 | 36.0 ± 1.9 | **1.32×** | 30.4 ± 1.1 | 154.9 ± 2.7 | 1.18× | 4.30× |
| Recursive Fibonacci | 21.6 ± 1.2 | 18.7 ± 0.6 | **1.16×** | 15.1 ± 0.6 | 49.8 ± 2.9 | 1.23× | 2.67× |

Geometric-mean stack→register speedup for these eight paired tests: **1.85×**.

### Interpretation

- **The rewrite paid off.** Four of the eight workloads are more than 2× faster; none regressed in this suite.
- **Locals and branches benefited most.** This fits a register VM reducing explicit stack manipulation and improving operand access patterns.
- **Calls and recursion improved less.** The recursive Fibonacci gap is only 1.16× stack→register, so call frames, dispatch, and recursive control flow likely dominate more of that workload than operand movement alone.
- **Global access is a designed win.** Oklang takes about 138 ms vs Lua’s 218 ms (~1.58× faster), consistent with compile-time-resolved globals loading by index rather than doing Lua-style environment-table access. It is evidence for your current static-global path, not a comparison of dynamic name lookups.

## 2. Numeric `for` loops

| Workload | Oklang (ms) | Lua (ms) | Python (ms) | Oklang/Lua time | Python/Oklang time |
|---|---:|---:|---:|---:|---:|
| Numeric-for counter | 146.2 ± 1.6 | 76.3 ± 0.9 | 1575.7 ± 31.8 | 1.92× | 10.78× |
| Numeric-for sum | 146.0 ± 1.1 | 76.1 ± 1.4 | 1639.2 ± 26.7 | 1.92× | 11.22× |
| Numeric-for step = 2 | 75.3 ± 0.9 | 41.8 ± 5.5 | 835.8 ± 9.8 | 1.80× | 11.10× |

### Interpretation

- Oklang’s numeric-for tests take about **1.8–1.9× Lua’s time**, but about **11× less time than Python’s** on these programs.
- The most apples-to-apples comparison is `numeric_for_sum` versus `arithmetic_sum`: they calculate the same sum over 10 million values, but the former uses numeric-for and the latter uses a general while loop. Mean runtime falls from 235.2→146.0 ms in Oklang (**1.61× faster**), 185.8→76.1 ms in Lua (**2.44× faster**), and 2,206→1,639 ms in Python (**1.35× faster**). Specialized iteration helps all three, with Lua gaining the most.
- `numeric_for_step` uses step 2 and therefore performs about 5 million iterations, half as many as the other numeric-for tests. Lua’s data has one obvious high outlier (57.2 ms); its median is 39.9 ms compared with the 41.8 ms mean, so the mean-based Lua comparison here is a little noisy.
- This does not mean Oklang’s numeric-for is poorly designed; it means Lua’s optimized numeric loop path is faster in this implementation. Bytecode and VM profiling are the next way to identify what remains per iteration.

## 3. Arrays

| Workload | Oklang (ms) | Lua (ms) | Python (ms) | Oklang/Lua time | Python/Oklang time |
|---|---:|---:|---:|---:|---:|
| Indexed array sum | 342.7 ± 8.1 | 276.7 ± 6.1 | 1998.4 ± 14.2 | 1.24× | 5.83× |
| Array for-in sum | 267.4 ± 2.5 | 492.3 ± 5.8 | 1332.3 ± 11.2 | 0.54× | 4.98× |
| Array push/growth | 17.5 ± 0.6 | 31.0 ± 1.7 | 77.2 ± 3.0 | 0.57× | 4.40× |
| Array literal allocation | 53.2 ± 3.4 | 47.2 ± 3.3 | 147.9 ± 5.1 | 1.13× | 2.78× |
| Array element: ordinary assignment | 96.5 ± 1.7 | 89.5 ± 4.6 | 415.8 ± 5.2 | 1.08× | 4.31× |
| Array element: compound assignment | 92.7 ± 1.6 | 88.8 ± 2.9 | 384.7 ± 6.0 | 1.04× | 4.15× |

### Interpretation

- **`for-in` is the strongest array result:** Oklang is about **1.84× faster than Lua** and about 5× faster than Python. The Lua version uses `ipairs`, which carries iterator-function/iterator-state overhead, while Oklang has its own array iteration path. This is a useful idiomatic benchmark, but a second Lua version using `for i = 1, #a do sum = sum + a[i] end` would better isolate iteration strategy from `ipairs` overhead.
- **Push/growth is another win:** Oklang takes 17.5 ms vs Lua’s 31.0 ms and Python’s 77.2 ms. The programs use idiomatic operations (`push(a, i)`, Lua `a[#a + 1] = i`, and Python `a.append(i)`), so this measures the whole language/runtime path, not identical internal operations. Lua’s version computes the next index via `#a` each time.
- **Indexed reads are respectable:** Oklang takes about 1.24× Lua’s time for repeated reads from an eight-element array. That is a relatively small gap considering the VM is still young.
- **Array literal allocation is close to Lua:** about 53.2 ms vs 47.2 ms, with both much faster than Python in this test. The three runtimes use different container representations and garbage-collection/reference-counting behavior, so this is a system comparison rather than a direct allocator microbenchmark.
- For indexed read/write, Lua uses 1-based table indices while Oklang and Python use zero-based indices; the scripts target corresponding elements.

## 4. Compound assignment versus ordinary assignment

These pairs repeat the same scalar update loop but switch between `x = x op operand` and `x op= operand`. The Oklang deltas below are computed from the two measured means. Negative means compound assignment was faster; positive means it was slower.

| Operation | Oklang plain (ms) | Oklang compound (ms) | Compound change | Python plain (ms) | Python compound (ms) | Python compound change |
|---|---:|---:|---:|---:|---:|---:|
| Addition | 120.4 ± 5.1 | 120.0 ± 5.3 | **-0.38%** | 971.7 | 980.8 | +0.94% |
| Subtraction | 119.4 ± 2.7 | 119.6 ± 5.4 | **+0.16%** | 969.6 | 967.6 | -0.20% |
| Multiplication | 51.3 ± 2.7 | 49.9 ± 1.1 | **-2.70%** | 430.2 | 428.6 | -0.38% |
| Division | 52.9 ± 1.4 | 53.3 ± 1.5 | **+0.73%** | 445.9 | 453.3 | +1.66% |
| Array element `a[0]` + 1 | 96.5 ± 1.7 | 92.7 ± 1.6 | **-3.96%** | 415.8 | 384.7 | -7.48% |

### Interpretation

- **There is no visible scalar compound-assignment penalty in these results.** Oklang `+=` is 0.38% faster, `-=` 0.16% slower, `*=` 2.70% faster, and `/=` 0.73% slower. Those are all small compared with the reported run-to-run spread; do not conclude that `*=` is genuinely faster or `/=` genuinely slower from this alone.
- **Array subscript `+=` is about 3.96% faster** than `a[0] = a[0] + 1` in Oklang. That one is a plausible real benefit because compound subscript assignment can resolve/evaluate the target only once, whereas the ordinary form expresses a read and a separate store. It is worth confirming via disassembly or repeated paired runs.
- Python’s scalar results also show only tiny changes between ordinary and augmented assignment. In the Python array-subscript test, `a[0] += 1` is about 7.5% faster than `a[0] = a[0] + 1`.
- Lua has no `+=`/`-=`/`*=`/`/=` syntax. The Lua “compound” benchmark files therefore use ordinary assignment too; their pair is only a cross-run baseline, **not** a measurement of compound-assignment syntax in Lua.
- These loops still include the loop test and the separate `i = i + 1` update on every iteration. If the goal is to identify a tiny operator-path cost, compare the emitted bytecode and consider an empty/control loop to understand the fixed loop overhead.

## 5. Python comparison

Across these particular programs, Python is slower than Oklang in all 25 result sets. The ratio ranges from about **2.7×** (recursive Fibonacci and array-literal allocation) to about **12.4×** (local-variable pressure). The numeric-for benchmarks show roughly 10.8–11.2× longer Python runtimes, and the arithmetic sum is about 9.4× longer.

This is a useful baseline for a small tree-walk-free / bytecode interpreter, but it is not a universal statement about Python. The tested Python code uses ordinary interpreter-level loops and per-element operations; Python workloads that delegate work to optimized C libraries are a different class of benchmark.

## 6. Memory field in the reports

The raw JSON includes `memory_usage_byte`. Across the recorded runs, its median across benchmark entries is approximately 3.13 MiB for Oklang register and Lua, and approximately 11.56 MiB for Python. Array-growth workloads increase the recorded values for all three, with the largest recorded values around 11.7 MiB for Oklang/Lua and 19.2 MiB for Python. The exact measurement method is not documented in the combined report, so interpret this as a rough process-level metric rather than language-managed allocation totals.

## 7. Recommended next steps

1. **Keep this suite as a regression baseline.** It now spans scalar execution, control flow, globals, calls/closures, numeric-for, array access/iteration/growth/literals, and compound-assignment forms.
2. **Confirm output correctness separately.** All runs exited with code 0, but the combined report does not record output matching. Compare expected checksums/results, allowing for Oklang’s `%g` versus other print formatting.
3. **Inspect bytecode for numeric-for and branch workloads.** `numeric_for_sum` is noticeably faster than the general while-loop sum, but Lua still has about a 2.44× advantage on this pair. Count instructions per iteration and check for unnecessary moves/materialized conditions.
4. **Inspect local-pressure code generation.** Oklang is only 13% slower than Lua here and already 2.67× faster than the old stack VM. This is a good focused optimization target with a substantial stack→register win behind it.
5. **Add a numeric-index Lua array traversal comparator.** Keep the current `ipairs` test as an idiomatic test, but add an explicit index loop to separate the cost of Lua’s generic iterator protocol from the cost of walking the array.
6. **Treat compound assignment as “not yet measurably slower,” not “proven identical.”** The scalar differences are too small to support a strong claim either way; array-subscript `+=` looks promising. Inspect the emitted instructions to understand why.

## Full result inventory

| Benchmark | Oklang register (ms) | Lua (ms) | Python (ms) | Stack VM (ms) |
|---|---:|---:|---:|---:|
| Loop counter | 194.3 ± 3.5 | 139.1 ± 1.8 | 1110.7 ± 14.8 | 403.9 ± 4.9 |
| Arithmetic sum | 235.2 ± 3.0 | 185.8 ± 1.9 | 2206.4 ± 67.6 | 534.1 ± 8.7 |
| Branch toggle | 179.2 ± 4.0 | 133.0 ± 7.7 | 1388.8 ± 22.3 | 451.1 ± 7.1 |
| Local-variable pressure | 67.2 ± 1.4 | 59.5 ± 1.9 | 830.7 ± 27.3 | 179.4 ± 2.6 |
| Global access | 137.9 ± 7.8 | 217.8 ± 4.8 | 551.5 ± 12.6 | 265.0 ± 1.5 |
| Function calls | 55.5 ± 1.4 | 51.1 ± 8.9 | 293.2 ± 12.2 | 81.5 ± 3.5 |
| Closure/upvalue | 36.0 ± 1.9 | 30.4 ± 1.1 | 154.9 ± 2.7 | 47.4 ± 1.7 |
| Recursive Fibonacci | 18.7 ± 0.6 | 15.1 ± 0.6 | 49.8 ± 2.9 | 21.6 ± 1.2 |
| Numeric-for counter | 146.2 ± 1.6 | 76.3 ± 0.9 | 1575.7 ± 31.8 | — |
| Numeric-for sum | 146.0 ± 1.1 | 76.1 ± 1.4 | 1639.2 ± 26.7 | — |
| Numeric-for step = 2 | 75.3 ± 0.9 | 41.8 ± 5.5 | 835.8 ± 9.8 | — |
| Indexed array sum | 342.7 ± 8.1 | 276.7 ± 6.1 | 1998.4 ± 14.2 | — |
| Array for-in sum | 267.4 ± 2.5 | 492.3 ± 5.8 | 1332.3 ± 11.2 | — |
| Array push/growth | 17.5 ± 0.6 | 31.0 ± 1.7 | 77.2 ± 3.0 | — |
| Array literal allocation | 53.2 ± 3.4 | 47.2 ± 3.3 | 147.9 ± 5.1 | — |
| Array element: ordinary assignment | 96.5 ± 1.7 | 89.5 ± 4.6 | 415.8 ± 5.2 | — |
| Array element: compound assignment | 92.7 ± 1.6 | 88.8 ± 2.9 | 384.7 ± 6.0 | — |
| Addition plain | 120.4 ± 5.1 | 90.7 ± 2.9 | 971.7 ± 21.3 | — |
| Addition compound | 120.0 ± 5.3 | 89.8 ± 1.4 | 980.8 ± 26.7 | — |
| Subtraction plain | 119.4 ± 2.7 | 90.4 ± 1.7 | 969.6 ± 7.1 | — |
| Subtraction compound | 119.6 ± 5.4 | 89.5 ± 1.8 | 967.6 ± 14.8 | — |
| Multiplication plain | 51.3 ± 2.7 | 42.0 ± 1.4 | 430.2 ± 9.8 | — |
| Multiplication compound | 49.9 ± 1.1 | 42.3 ± 1.2 | 428.6 ± 7.8 | — |
| Division plain | 52.9 ± 1.4 | 38.8 ± 1.4 | 445.9 ± 10.9 | — |
| Division compound | 53.3 ± 1.5 | 38.9 ± 1.2 | 453.3 ± 10.1 | — |

---

Source data: `combined_output.json` and `combined_output.md` uploaded with this conversation. Computed ratios in this report are derived from the recorded means.
