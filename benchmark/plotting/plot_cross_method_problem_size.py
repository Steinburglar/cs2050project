#!/usr/bin/env python3
"""Plot problem-size scaling from several benchmark CSVs on one figure."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from plot_single_csv import compute_metric, read_csv


DEFAULT_SERIES = [
    ("serial", Path("serial/results/serial_scaling.csv"), "N", "build_ms"),
    ("openmp", Path("openmp/results/openmp_strong.csv"), "N", "build_ms"),
    ("julia", Path("julia/results/julia_strong.csv"), "N", "build_ms"),
    ("mpi", Path("mpi/results/mpi_strong.csv"), "N", "build_ms"),
    ("cuda", Path("cuda/results/cuda_problem_size.csv"), "N", "build_ms"),
]


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--title", default="Cross-Method Problem-Size Scaling", help="plot title")
    parser.add_argument("--metric", default="time", choices=["time", "speedup", "efficiency"], help="y metric")
    parser.add_argument("--x-scale", default="log", choices=["lin", "log"], help="x-axis scale")
    parser.add_argument("--y-scale", default="log", choices=["lin", "log"], help="y-axis scale")
    parser.add_argument("--xlabel", default="N", help="x-axis label")
    parser.add_argument("--ylabel", default=None, help="y-axis label override")
    parser.add_argument("--y-col", default="build_ms", help="CSV column name for time data")
    parser.add_argument(
        "--series",
        action="append",
        nargs=4,
        metavar=("LABEL", "CSV", "XCOL", "YCOL"),
        help="extra series as label csv xcol ycol; may be repeated",
    )
    parser.add_argument(
        "--out",
        default="cross_method_problem_size.png",
        help="output image file name",
    )
    return parser.parse_args()


def default_ylabel(metric: str, y_col: str) -> str:
    if metric == "speedup":
        return "Speedup"
    if metric == "efficiency":
        return "Efficiency"
    base = y_col
    if base.endswith("_ms"):
        base = base[:-3].replace("_", " ").strip().title()
        return f"{base} Time (ms)"
    return y_col


def main() -> int:
    args = parse_args()

    series = list(DEFAULT_SERIES)
    if args.series:
        for label, csv_path, x_col, y_col in args.series:
            series.append((label, Path(csv_path), x_col, y_col))

    fig, ax = plt.subplots(figsize=(7.5, 4.8))

    for label, csv_path, x_col, y_col in series:
        if not csv_path.exists():
            print(f"Skipping missing CSV: {csv_path}")
            continue
        xs, times = read_csv(csv_path, x_col, y_col)
        ys = compute_metric(xs, times, args.metric)
        ax.plot(xs, ys, marker="o", linewidth=2, label=label)

    ax.set_title(args.title)
    ax.set_xlabel(args.xlabel)
    ylabel = default_ylabel(args.metric, args.y_col)
    if args.ylabel is not None:
        print(f"Warning: overriding auto y-label {ylabel!r} with {args.ylabel!r}")
        ylabel = args.ylabel
    ax.set_ylabel(ylabel)
    ax.set_xscale("log" if args.x_scale == "log" else "linear")
    ax.set_yscale("log" if args.y_scale == "log" else "linear")
    ax.grid(True, which="both", linestyle="--", alpha=0.4)
    ax.legend()
    fig.tight_layout()

    out_dir = Path("benchmark/results/figures")
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / args.out
    fig.savefig(out_path, dpi=200)
    print(f"Wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
