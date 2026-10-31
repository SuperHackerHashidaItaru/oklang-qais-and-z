#!/usr/bin/env python3
"""Generate benchmark charts from Hyperfine JSON exports."""

import argparse
import json
import math
from pathlib import Path

import matplotlib
matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np


IMPLEMENTATION_COLORS = {
    "ok-register": "#2563eb",
    "ok-stack": "#7c3aed",
    "lua": "#16a34a",
    "python3": "#d97706",
}


def load_benchmarks(results_dir: Path):
    benchmarks = {}

    for path in sorted(results_dir.glob("*.json")):
        try:
            with path.open(encoding="utf-8") as handle:
                document = json.load(handle)
        except (OSError, json.JSONDecodeError) as exc:
            raise SystemExit(f"Cannot read {path}: {exc}") from exc

        rows = document.get("results", [])
        if not isinstance(rows, list):
            print(f"Skipping {path.name}: 'results' is not a list")
            continue

        measurements = {}
        for row in rows:
            if not isinstance(row, dict):
                continue

            implementation = row.get("command")
            mean = row.get("mean")
            stddev = row.get("stddev", 0.0)
            median = row.get("median", mean)

            if not implementation or not isinstance(mean, (int, float)):
                continue
            if not math.isfinite(mean) or mean <= 0:
                continue
            if not isinstance(stddev, (int, float)) or not math.isfinite(stddev):
                stddev = 0.0
            if not isinstance(median, (int, float)) or not math.isfinite(median):
                median = mean

            measurements[str(implementation)] = {
                "mean": float(mean),
                "stddev": max(0.0, float(stddev)),
                "median": float(median),
            }

        if measurements:
            benchmarks[path.stem] = measurements
        else:
            print(f"Skipping {path.name}: no valid measurements")

    return benchmarks


def pretty_name(name: str) -> str:
    return name.replace("_", " ")


def color_for(implementation: str, index: int):
    if implementation in IMPLEMENTATION_COLORS:
        return IMPLEMENTATION_COLORS[implementation]
    return plt.get_cmap("tab10")(index % 10)


def plot_one_benchmark(name, measurements, output_dir):
    implementations = list(measurements)
    means_ms = [measurements[key]["mean"] * 1000 for key in implementations]
    errors_ms = [measurements[key]["stddev"] * 1000 for key in implementations]
    colors = [color_for(key, i) for i, key in enumerate(implementations)]

    width = max(7.5, 1.5 * len(implementations))
    fig, ax = plt.subplots(figsize=(width, 5.2))
    bars = ax.bar(
        implementations,
        means_ms,
        yerr=errors_ms,
        capsize=5,
        color=colors,
        alpha=0.9,
        edgecolor="white",
        linewidth=0.7,
    )

    ax.set_title(pretty_name(name), fontsize=15, pad=14)
    ax.set_ylabel("Mean execution time (ms)")
    ax.set_xlabel("Implementation")
    ax.grid(axis="y", alpha=0.25)
    ax.set_axisbelow(True)

    for bar, value in zip(bars, means_ms):
        ax.annotate(
            f"{value:.3f}",
            (bar.get_x() + bar.get_width() / 2, bar.get_height()),
            xytext=(0, 6),
            textcoords="offset points",
            ha="center",
            va="bottom",
            fontsize=9,
        )

    fig.tight_layout()
    fig.savefig(output_dir / f"{name}.png", dpi=180, bbox_inches="tight")
    fig.savefig(output_dir / f"{name}.svg", bbox_inches="tight")
    plt.close(fig)


def plot_absolute_overview(benchmarks, output_dir):
    benchmark_names = list(benchmarks)
    implementations = sorted({
        implementation
        for measurements in benchmarks.values()
        for implementation in measurements
    })

    x = np.arange(len(benchmark_names))
    group_width = 0.82
    bar_width = group_width / max(1, len(implementations))

    fig_width = max(12, len(benchmark_names) * 0.65)
    fig, ax = plt.subplots(figsize=(fig_width, 7))

    for index, implementation in enumerate(implementations):
        values = []
        for benchmark in benchmark_names:
            result = benchmarks[benchmark].get(implementation)
            values.append(result["mean"] * 1000 if result else np.nan)

        offset = (index - (len(implementations) - 1) / 2) * bar_width
        ax.bar(
            x + offset,
            values,
            width=bar_width * 0.92,
            label=implementation,
            color=color_for(implementation, index),
        )

    ax.set_xticks(x)
    ax.set_xticklabels(benchmark_names, rotation=65, ha="right", fontsize=8)
    ax.set_ylabel("Mean execution time (ms; logarithmic scale)")
    ax.set_yscale("log")
    ax.set_title("Benchmark execution times")
    ax.grid(axis="y", which="both", alpha=0.22)
    ax.set_axisbelow(True)
    ax.legend(ncol=min(4, len(implementations)), loc="best")
    fig.tight_layout()
    fig.savefig(output_dir / "overview.png", dpi=200, bbox_inches="tight")
    fig.savefig(output_dir / "overview.svg", bbox_inches="tight")
    plt.close(fig)


def plot_relative_overview(benchmarks, output_dir):
    benchmark_names = list(benchmarks)
    implementations = sorted({
        implementation
        for measurements in benchmarks.values()
        for implementation in measurements
    })

    matrix = np.full(
        (len(benchmark_names), len(implementations)),
        np.nan,
        dtype=float,
    )

    for row, benchmark in enumerate(benchmark_names):
        measurements = benchmarks[benchmark]
        fastest = min(item["mean"] for item in measurements.values())

        for column, implementation in enumerate(implementations):
            item = measurements.get(implementation)
            if item:
                matrix[row, column] = item["mean"] / fastest

    fig_width = max(9, len(implementations) * 2.0)
    fig_height = max(6, len(benchmark_names) * 0.38)
    fig, ax = plt.subplots(figsize=(fig_width, fig_height))

    image = ax.imshow(matrix, aspect="auto", cmap="RdYlGn_r", vmin=1)
    ax.set_xticks(range(len(implementations)))
    ax.set_xticklabels(implementations)
    ax.set_yticks(range(len(benchmark_names)))
    ax.set_yticklabels(benchmark_names, fontsize=8)
    ax.set_xlabel("Implementation")
    ax.set_ylabel("Benchmark")
    ax.set_title("Relative execution time (1.00x = fastest for that benchmark)")

    for row in range(matrix.shape[0]):
        for column in range(matrix.shape[1]):
            value = matrix[row, column]
            if np.isfinite(value):
                label = f"{value:.2f}x"
                ax.text(
                    column, row, label,
                    ha="center", va="center", fontsize=8,
                    color="black",
                )

    fig.colorbar(image, ax=ax, label="Multiple of fastest implementation")
    fig.tight_layout()
    fig.savefig(output_dir / "overview-relative.png", dpi=200, bbox_inches="tight")
    fig.savefig(output_dir / "overview-relative.svg", bbox_inches="tight")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(
        description="Plot all Hyperfine JSON benchmark results."
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=Path("results/latest"),
        help="Directory containing Hyperfine .json exports",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Plot output directory (default: INPUT/plot)",
    )
    args = parser.parse_args()

    if not args.input.is_dir():
        raise SystemExit(f"Input directory does not exist: {args.input}")

    output_dir = args.output or args.input / "plot"
    benchmark_dir = output_dir / "benchmarks"
    benchmark_dir.mkdir(parents=True, exist_ok=True)

    benchmarks = load_benchmarks(args.input)
    if not benchmarks:
        raise SystemExit(f"No valid Hyperfine JSON results in {args.input}")

    for name, measurements in benchmarks.items():
        plot_one_benchmark(name, measurements, benchmark_dir)

    plot_absolute_overview(benchmarks, output_dir)
    plot_relative_overview(benchmarks, output_dir)

    print(f"Generated plots for {len(benchmarks)} benchmarks.")
    print(f"Output: {output_dir}")


if __name__ == "__main__":
    main()
