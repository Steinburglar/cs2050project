#!/usr/bin/env python3
"""Compare a generated edge CSV against the expected validation output."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def read_edges(path: Path):
    edges = []
    with path.open("r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(",")
            if len(parts) != 2:
                raise ValueError(f"invalid edge line in {path}: {line!r}")
            edges.append((int(parts[0]), int(parts[1])))
    return edges


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("expected", type=Path)
    parser.add_argument("actual", type=Path)
    args = parser.parse_args()

    expected = read_edges(args.expected)
    actual = read_edges(args.actual)

    if expected != actual:
        print("Validation failed: edge lists do not match.", file=sys.stderr)
        print(f"Expected ({len(expected)} edges): {expected}", file=sys.stderr)
        print(f"Actual   ({len(actual)} edges): {actual}", file=sys.stderr)
        return 1

    print(f"Validation passed: {len(actual)} edges matched.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
