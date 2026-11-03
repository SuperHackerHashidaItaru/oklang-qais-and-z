#!/usr/bin/env python3
"""delta blue comparison: oklang vs wren vs python3.

borrowed benchmark: wren's repo ships delta_blue in wren and python
(wren/test/benchmark/delta_blue.{wren,py}); this is the oklang port
(benchmarks/delta_blue.ok) plus the comparison runner.

methodology: external wall-clock, 2 warmup + N timed runs each, report
min/median/max. outputs are cross-checked: the totals must agree
numerically (oklang prints %g, the others print integers).

env overrides: OK_BIN (release build), OK_THREADED_BIN, WREN_BIN, PY_BIN,
BENCH_DIR, RUNS.
"""
import os
import statistics
import subprocess
import sys
import time

BENCH = os.environ.get("BENCH_DIR", os.path.join(os.path.dirname(os.path.abspath(__file__)), "benchmarks"))
RUNS = int(os.environ.get("RUNS", "10"))
WARMUP = 2

OK = os.environ.get("OK_BIN", "./ok")
OK_THREADED = os.environ.get("OK_THREADED_BIN", "")
WREN = os.environ.get("WREN_BIN", "wren_cli")
PY = os.environ.get("PY_BIN", "python3")

TARGETS = [("ok", [OK, f"{BENCH}/delta_blue.ok"])]
if OK_THREADED:
    TARGETS.append(("ok-threaded", [OK_THREADED, f"{BENCH}/delta_blue.ok"]))
TARGETS.append(("wren", [WREN, f"{BENCH}/delta_blue.wren"]))
TARGETS.append(("python", [PY, f"{BENCH}/delta_blue.py"]))


def run_once(cmd):
    t0 = time.perf_counter()
    r = subprocess.run(cmd, capture_output=True, text=True)
    dt = (time.perf_counter() - t0) * 1000.0
    if r.returncode != 0:
        print(f"FAILED: {' '.join(cmd)}\n{r.stderr[:500]}", file=sys.stderr)
        sys.exit(1)
    return dt, r.stdout.strip().splitlines()


totals = {}
results = {}
for name, cmd in TARGETS:
    for _ in range(WARMUP):
        run_once(cmd)
    times = []
    out_lines = []
    for _ in range(RUNS):
        dt, out_lines = run_once(cmd)
        times.append(dt)
    results[name] = times
    first = out_lines[0] if out_lines else "?"
    totals[name] = float(first) if first not in ("?", "") else None
    print(f"{name:14s} median {statistics.median(times):8.1f} ms   min {min(times):8.1f}   max {max(times):8.1f}   total: {first}")

# cross-check: all totals must agree numerically
vals = [v for v in totals.values() if v is not None]
ref = vals[0] if vals else 0.0
ok = all(abs(v - ref) <= 1e-5 * max(1.0, abs(ref)) for v in vals)
print(f"\ncross-check totals: {'PASS' if ok else 'FAIL'} ({ref:g})")
if not ok:
    sys.exit(1)

if "wren" in results:
    ref_med = statistics.median(results["wren"])
    print()
    for name in results:
        med = statistics.median(results[name])
        print(f"{name:14s} {med/ref_med:6.2f}x of wren")
