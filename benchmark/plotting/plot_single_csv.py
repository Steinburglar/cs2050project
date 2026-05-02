#!/usr/bin/env python3
"""Plot a single labeled CSV as x vs y."""

from __future__ import annotations

import argparse
import math
from pathlib import Path

import matplotlib.pyplot as plt


def read_csv(path: Path, x_col: str, y_col: str):
    xs = []
    ys = []
    with path.open("r", encoding="utf-8") as f:
        header = None
        x_idx = None
        y_idx = None
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(",")
            if header is None:
                header = parts
                try:
                    x_idx = header.index(x_col)
                    y_idx = header.index(y_col)
                except ValueError as exc:
                    raise ValueError(
                        f"missing column in {path}: need x={x_col!r}, y={y_col!r}, header={header!r}"
                    ) from exc
                continue
            if len(parts) <= max(x_idx, y_idx):
                continue
            xs.append(float(parts[x_idx]))
            ys.append(float(parts[y_idx]))
    return xs, ys


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path, help="input CSV with two columns")
    parser.add_argument("--title", required=True, help="plot title")
    parser.add_argument("--x-scale", default="lin", choices=["lin", "log"], help="x-axis scale")
    parser.add_argument("--y-scale", default="lin", choices=["lin", "log"], help="y-axis scale")
    parser.add_argument("--xlabel", default="N", help="x-axis label")
    parser.add_argument("--ylabel", default="Time (ms)", help="y-axis label")
    parser.add_argument("--x-col", default="N", help="CSV column name for x data")
    parser.add_argument("--y-col", default="total_ms", help="CSV column name for y data")
    parser.add_argument("--fit-slope", action="store_true", help="fit slope in selected scale space")
    parser.add_argument(
        "--ref-exp",
        type=float,
        default=None,
        help="plot dotted reference line with this exponent",
    )
    parser.add_argument(
        "--ref-label",
        default=None,
        help="label for reference line; defaults to N^exp",
    )
    parser.add_argument("--out", required=True, help="output image file name")
    return parser.parse_args()


def scale_value(value: float, scale: str) -> float:
    if scale == "log":
        if value <= 0:
            raise ValueError("log scale needs positive values")
        return math.log10(value)
    return value


def inverse_scale_value(value: float, scale: str) -> float:
    if scale == "log":
        return 10 ** value
    return value


def main() -> int:
    args = parse_args()
    xs, ys = read_csv(args.csv, args.x_col, args.y_col)
    out_dir = args.csv.parent / "figures"
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / args.out

    slope_text = None
    if args.fit_slope:
        sx = [scale_value(x, args.x_scale) for x in xs]
        sy = [scale_value(y, args.y_scale) for y in ys]
        n = len(sx)
        mean_x = sum(sx) / n
        mean_y = sum(sy) / n
        num = sum((x - mean_x) * (y - mean_y) for x, y in zip(sx, sy))
        den = sum((x - mean_x) ** 2 for x in sx)
        slope = num / den if den != 0 else float("nan")
        slope_text = f"slope = {slope:.4f}"
        print(slope_text)

    fig, ax = plt.subplots(figsize=(7, 4.5))
    ax.plot(xs, ys, marker="o", linewidth=2)
    ax.set_title(args.title)
    ax.set_xlabel(args.xlabel)
    ax.set_ylabel(args.ylabel)
    ax.set_xscale("log" if args.x_scale == "log" else "linear")
    ax.set_yscale("log" if args.y_scale == "log" else "linear")
    ax.grid(True, which="both", linestyle="--", alpha=0.4)
    if slope_text is not None:
        ax.text(0.05, 0.95, slope_text, transform=ax.transAxes, va="top")

    if args.ref_exp is not None:
        x_min = min(xs)
        x_max = max(xs)
        if x_min == x_max:
            ref_xs = [x_min, x_max]
        else:
            ref_xs = [x_min, x_max]
        base_x = ref_xs[0]
        base_y = ys[0]
        ref_ys = [
            base_y * (x / base_x) ** args.ref_exp
            for x in ref_xs
        ]
        ref_label = args.ref_label or f"N^{args.ref_exp:g}"
        ax.plot(ref_xs, ref_ys, linestyle=":", linewidth=2, label=ref_label)
        ax.legend()
    fig.tight_layout()
    fig.savefig(out_path, dpi=200)
    print(f"Wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
