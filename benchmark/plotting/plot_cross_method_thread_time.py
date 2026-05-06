#!/usr/bin/env python3
"""Plot total runtime vs thread count for multiple methods."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from plot_single_csv import read_csv


DEFAULT_SERIES = [
    ("OpenMP", Path("report/data/openmp_strong.csv"), "nthreads", "total_ms"),
    ("Julia", Path("report/data/julia_strong.csv"), "nthreads", "total_ms"),
]


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--title", default="Total Runtime vs Thread Count", help="plot title")
    parser.add_argument("--x-scale", default="log", choices=["lin", "log"], help="x-axis scale")
    parser.add_argument("--y-scale", default="log", choices=["lin", "log"], help="y-axis scale")
    parser.add_argument("--xlabel", default="Threads", help="x-axis label")
    parser.add_argument("--ylabel", default="Total Time (ms)", help="y-axis label")
    parser.add_argument(
        "--series",
        action="append",
        nargs=4,
        metavar=("LABEL", "CSV", "XCOL", "YCOL"),
        help="extra series as label csv xcol ycol; may be repeated",
    )
    parser.add_argument(
        "--out",
        default="openmp_vs_julia_strong_total.png",
        help="output image file name",
    )
    return parser.parse_args()


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
        xs, ys = read_csv(csv_path, x_col, y_col)
        ax.plot(xs, ys, marker="o", linewidth=2, label=label)

    ax.set_title(args.title)
    ax.set_xlabel(args.xlabel)
    ax.set_ylabel(args.ylabel)
    ax.set_xscale("log" if args.x_scale == "log" else "linear")
    ax.set_yscale("log" if args.y_scale == "log" else "linear")
    ax.grid(True, which="both", linestyle="--", alpha=0.4)
    ax.legend()
    fig.tight_layout()

    out_dir = Path("report/figures")
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / args.out
    fig.savefig(out_path, dpi=200)
    print(f"Wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
