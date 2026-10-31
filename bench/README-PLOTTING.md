# Hyperfine benchmark pipeline

This bundle contains the benchmark runner, plotting script, result publisher,
stack-VM compatibility list, and GitHub Actions workflow.

## Copy into your repository

- Copy `bench/run-benchmarks.sh` to your `bench/` directory.
- Copy `bench/plot.py` to `bench/`.
- Copy `bench/publish-results.py` to `bench/`.
- Copy `bench/stack-compatible.txt` to `bench/`.
- Copy `.github/workflows/benches.yml` to your repository's `.github/workflows/`.

Keep your existing `bench/combine.py`; the workflow continues to use it.

## Add a benchmark

Add all three matching files under `bench/benchmarks/`:

- `new_benchmark.ok`
- `new_benchmark.lua`
- `new_benchmark.py`

The runner discovers `.ok` files automatically and requires matching `.lua` and
`.py` implementations. No benchmark-name list needs updating.

If the benchmark supports your optional stack VM, add `new_benchmark` to
`bench/stack-compatible.txt`. If `OK_STACK` is empty, it is not run.

## Local usage

From the repository root:

```bash
sudo apt-get install hyperfine lua5.4 python3-matplotlib
cd bench
OK_REGISTER=/path/to/ok ./run-benchmarks.sh benchmarks results/_staging
python3 combine.py -s results/_staging -o results/_staging/combined.md -e .md
python3 plot.py --input results/_staging
```

Write `results/_staging/run-info.txt` before publishing. It must contain a
`date:` line and should contain an `oklang_ref:` line. Then publish:

```bash
python3 publish-results.py --results results --keep 3
```

`latest/` is the current successful run. `archive/` retains the three previous
successful runs, so there can be four complete runs total including `latest/`.

## Plot outputs

- `plot/overview.png` and `.svg`: absolute times, logarithmic Y axis.
- `plot/overview-relative.png` and `.svg`: per-benchmark relative performance.
- `plot/benchmarks/<name>.png` and `.svg`: one chart per benchmark.

Raw Hyperfine JSON and Markdown exports are retained alongside the plots.
