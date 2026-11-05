#!/usr/bin/env python3
"""The baseline column (P6 round two): diff a results directory of hyperfine
JSON exports against the pinned baseline (bench/baseline.json), one markdown
table per direction - advisory, never a gate.

why advisory: the baseline is pinned from one runner instance and compared
against runs on other instances of the same github runner class; absolute
medians wobble a few percent across instances, so the column surfaces
shapes (a benchmark that jumped 30% is visible, a 4% wiggle is noise).

the re-baseline door is manual on purpose: an auto-moving baseline can
never catch a regression. after a landing that INTENDS to move the numbers
(a deliberate optimization, a re-baseline ruling), run:

    python3 baseline-diff.py --write results/_staging

review the diff, then commit baseline.json alongside the results.
"""
import argparse
import json
from pathlib import Path

MISSING = "—"


def load_medians(results_dir: Path):
    """benchmark -> {target: median_ms} from a directory of hyperfine JSONs."""
    benchmarks = {}
    for path in sorted(results_dir.glob("*.json")):
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            raise SystemExit(f"cannot read {path}: {exc}") from exc
        rows = document.get("results", [])
        medians = {}
        for row in rows:
            target = row.get("command")
            median = row.get("median", row.get("mean"))
            if target and isinstance(median, (int, float)) and median > 0:
                medians[str(target)] = float(median)
        if medians:
            benchmarks[path.stem] = medians
    return benchmarks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results_dir", type=Path, help="directory of hyperfine .json exports")
    parser.add_argument("--baseline", type=Path, default=Path(__file__).with_name("baseline.json"))
    parser.add_argument("--write", action="store_true",
                        help="(re)pin the baseline FROM this results dir instead of diffing")
    args = parser.parse_args()

    if not args.results_dir.is_dir():
        raise SystemExit(f"not a directory: {args.results_dir}")

    current = load_medians(args.results_dir)

    if args.write:
        baseline = {
            "meta": {
                "note": "pinned by baseline-diff.py --write; medians in ms. the column is advisory; see the script doc.",
            },
            "benchmarks": current,
        }
        args.baseline.write_text(json.dumps(baseline, indent=1, sort_keys=True) + "\n", encoding="utf-8")
        print(f"baseline pinned: {args.baseline} ({len(current)} benchmarks)")
        return

    if not args.baseline.is_file():
        raise SystemExit(f"no baseline at {args.baseline}; pin one first with --write")

    document = json.loads(args.baseline.read_text(encoding="utf-8"))
    pinned = document.get("benchmarks", {})

    lines = [
        "# baseline diff (advisory)",
        "",
        "current run vs the pinned baseline (`bench/baseline.json`). cross-instance",
        "noise is a few percent - read shapes, not decimals. the re-baseline door",
        "is `baseline-diff.py --write` + a human commit.",
        "",
        "| benchmark | target | baseline ms | current ms | delta |",
        "|---|---|---:|---:|---:|",
    ]
    for name in sorted(set(pinned) | set(current)):
        base_row = pinned.get(name, {})
        curr_row = current.get(name, {})
        for target in sorted(set(base_row) | set(curr_row)):
            base = base_row.get(target)
            curr = curr_row.get(target)
            if base is None:
                delta = MISSING + " (new target)"
            elif curr is None:
                delta = MISSING + " (absent)"
            else:
                delta = f"{(curr - base) / base * 100:+.1f}%"
            lines.append(
                f"| {name} | {target} | "
                f"{base if base is not None else MISSING} | "
                f"{curr if curr is not None else MISSING} | {delta} |"
            )

    output = args.results_dir / "baseline-diff.md"
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"baseline diff written: {output} ({len(current)} benchmarks vs {len(pinned)} pinned)")


if __name__ == "__main__":
    main()
