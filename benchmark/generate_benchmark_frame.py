#!/usr/bin/env python3
"""Generate a random benchmark extxyz frame for neighbor-list performance runs.

The output is a standard extxyz file:
- first line: atom count
- second line: comment with box / periodic metadata
- remaining lines: element symbol and coordinates

Positions are sampled uniformly within a cubic box. Use a fixed seed for
reproducibility.
"""

from __future__ import annotations

import argparse
import random
from pathlib import Path


def generate_positions(n: int, box: float, seed: int):
    rng = random.Random(seed)
    positions = []
    for _ in range(n):
        positions.append(
            (
                rng.random() * box,
                rng.random() * box,
                rng.random() * box,
            )
        )
    return positions


def parse_periodic_triplet(values):
    if len(values) != 3:
        raise ValueError("--periodic expects exactly three values")
    periodic = []
    for value in values:
        if value in {"1", "true", "True", "T", "t"}:
            periodic.append("T")
        elif value in {"0", "false", "False", "F", "f"}:
            periodic.append("F")
        else:
            raise ValueError(f"invalid periodicity value: {value}")
    return periodic


def write_xyz(path: Path, positions, box: float, periodic, symbol: str = "H") -> None:
    with path.open("w", encoding="utf-8") as f:
        f.write(f"{len(positions)}\n")
        f.write(
            'benchmark frame; '
            f'Lattice="{box:.6f} 0 0 0 {box:.6f} 0 0 0 {box:.6f}" '
            f'pbc="{periodic[0]} {periodic[1]} {periodic[2]}"\n'
        )
        for x, y, z in positions:
            f.write(f"{symbol} {x:.8f} {y:.8f} {z:.8f}\n")


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--n", type=int, default=10000, help="number of atoms")
    parser.add_argument("--box", type=float, default=50.0, help="cubic box length")
    parser.add_argument("--seed", type=int, default=42, help="random seed")
    parser.add_argument("--symbol", type=str, default="H", help="element symbol to write")
    parser.add_argument(
        "--periodic",
        nargs=3,
        default=["1", "1", "1"],
        help="three periodicity flags, e.g. 1 1 1 or T T T",
    )
    parser.add_argument("--out", type=Path, default=Path("benchmark.xyz"), help="output extxyz file")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    positions = generate_positions(args.n, args.box, args.seed)
    periodic = parse_periodic_triplet(args.periodic)
    write_xyz(args.out, positions, args.box, periodic, args.symbol)
    print(f"Wrote {args.n} atoms to {args.out}")


if __name__ == "__main__":
    main()
