#!/usr/bin/env python3
"""Generate a random sample frame CSV for testing.

Output format: x,y,z,species (commas allowed; loader tolerates commas or whitespace)
"""
import argparse
import random


def generate(n=100, box=50.0, species_choices=(1, 2, 3), seed=42):
    random.seed(seed)
    pts = []
    for i in range(n):
        x = random.random() * box
        y = random.random() * box
        z = random.random() * box
        sp = random.choice(species_choices)
        pts.append((x, y, z, sp))
    return pts


def write_csv(path, pts):
    with open(path, "w") as f:
        f.write("# x,y,z,species\n")
        for x, y, z, sp in pts:
            f.write(f"{x:.6f},{y:.6f},{z:.6f},{sp}\n")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--n", type=int, default=100)
    p.add_argument("--box", type=float, default=50.0)
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--out", type=str, default="sample_frame.csv")
    args = p.parse_args()

    pts = generate(n=args.n, box=args.box, seed=args.seed)
    write_csv(args.out, pts)
    print(f"Wrote {len(pts)} atoms to {args.out}")


if __name__ == "__main__":
    main()
