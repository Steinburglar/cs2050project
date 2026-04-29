#!/usr/bin/env python3
"""Generate the repository's validation set using ASE.

This script overwrites the repository validation pair:
- validation/validation_frame.xyz
- validation/validation_expected_edges.csv

The generated system intentionally mixes:
- hand-placed atoms near faces / edges / corners to exercise periodic wrapping
- local pairs just inside and just outside the cutoff
- additional random atoms placed with a minimum separation

Important:
- Ground-truth neighbor pairs come from ASE's own neighbor-list API.
- The local minimum-image helper below is only used while placing random atoms,
  so the generated frame is not overly clustered.

Design:
- The validation frame is not meant to look like a realistic equilibrated MD
  configuration. It is a correctness fixture.
- We intentionally seed it with hand-placed atoms that exercise corner cases:
  periodic wrap-around across faces, edges, and corners; pairs just inside the
  cutoff; and pairs just outside the cutoff.
- We then add random atoms to bring the system up to a more substantial size.
  Those random atoms are sampled uniformly in the box with no extra filtering.
"""

from __future__ import annotations

import argparse
import math
import random
from pathlib import Path

from ase import Atoms
from ase.io import write
from ase.neighborlist import neighbor_list

SCRIPT_DIR = Path(__file__).resolve().parent

def add_special_positions(box_length, cutoff):
    """Create hand-placed atoms that exercise specific neighbor-list cases.

    The returned coordinates are designed to cover several situations that are
    easy to miss if we rely only on a random cloud:

    - ordinary near neighbors well inside the box
    - pairs that become neighbors only because of periodic wrapping
    - atoms near faces, edges, and corners of the periodic box
    - pairs placed just inside the cutoff
    - pairs placed just outside the cutoff
    - a small diagonal-separation case so we do not only test axis-aligned
      geometry

    The `eps` margin keeps the "inside" and "outside" cases safely away from
    the exact cutoff, which makes the expected answer less sensitive to
    floating-point tie behavior.
    """
    eps = 0.05
    inside = cutoff - eps
    outside = cutoff + eps
    half = 0.5 * box_length

    positions = [
        (0.20, 0.20, 0.20),
        (0.20 + inside, 0.20, 0.20),
        (box_length - 0.30, 0.20, 0.20),
        (0.20, box_length - 0.35, 0.20),
        (0.20, 0.20, box_length - 0.40),
        (box_length - 0.30, box_length - 0.30, 0.20),
        (box_length - 0.35, box_length - 0.35, box_length - 0.35),
        (5.0, 5.0, 5.0),
        (5.0 + inside, 5.0, 5.0),
        (8.0 + outside, 5.0, 5.0),
        (box_length - 0.20, 10.0, 10.0),
        (outside - 0.20, 10.0, 10.0),
        (half, half, half),
        (half + inside / math.sqrt(3.0), half + inside / math.sqrt(3.0), half),
        (half + outside, half, half),
        (15.0, 15.0, 2.0),
        (15.0, 15.0 + inside, 2.0),
        (15.0, 15.0, 2.0 + inside),
        (box_length - 0.10, box_length - 0.10, 12.0),
        (0.40, 0.40, 12.0),
    ]
    return positions


def generate_positions(n_atoms, box_length, cutoff, seed):
    """Fill the validation box with corner cases first, then random atoms."""
    rng = random.Random(seed)
    positions = add_special_positions(box_length, cutoff)

    while len(positions) < n_atoms:
        positions.append(
            (
            rng.random() * box_length,
            rng.random() * box_length,
            rng.random() * box_length,
            )
        )

    return positions


def compute_half_neighbor_pairs_with_ase(atoms, cutoff):
    """Extract a half neighbor list directly from ASE's neighbor_list API.

    We use ASE's global-cutoff functional interface:

        neighbor_list("ij", atoms, cutoff)

    ASE's documentation states that atoms are mapped into the box under
    periodic boundary conditions, so this is the clearest way to request the
    exact neighbor definition used by our project.

    The returned arrays may contain both `(i, j)` and `(j, i)`, so we collapse
    them into sorted half-list pairs with `i < j`.
    """
    i_idx, j_idx = neighbor_list("ij", atoms, cutoff, self_interaction=False)
    pairs = {(min(int(i), int(j)), max(int(i), int(j))) for i, j in zip(i_idx, j_idx) if i != j}
    return sorted(pairs)


def write_edge_csv(path, pairs, cutoff):
    """Write the expected half neighbor list in the repository CSV format."""
    with path.open("w", encoding="utf-8") as f:
        f.write("# source,destination\n")
        f.write(f"# cutoff={cutoff}, periodic=true, reference=ASE\n")
        for i, j in pairs:
            f.write(f"{i},{j}\n")


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--n", type=int, default=100, help="number of atoms")
    parser.add_argument("--box", type=float, default=20.0, help="orthorhombic box length")
    parser.add_argument("--cutoff", type=float, default=5.0, help="neighbor cutoff")
    parser.add_argument("--seed", type=int, default=42, help="random seed")
    parser.add_argument(
        "--frame-out",
        type=Path,
        default=SCRIPT_DIR / "validation_frame.xyz",
        help="output extxyz file to replace the current validation frame",
    )
    parser.add_argument(
        "--edges-out",
        type=Path,
        default=SCRIPT_DIR / "validation_expected_edges.csv",
        help="output expected edge CSV to replace the current ground truth",
    )
    return parser.parse_args()


def main():
    """Generate the validation frame and overwrite the repository fixtures."""
    args = parse_args()
    if args.n < 20:
        raise ValueError("n must be at least 20 so the special corner-case atoms are included")

    positions = generate_positions(
        n_atoms=args.n,
        box_length=args.box,
        cutoff=args.cutoff,
        seed=args.seed,
    )

    atoms = Atoms(
        symbols=["Ar"] * len(positions),
        positions=positions,
        cell=[args.box, args.box, args.box],
        pbc=[True, True, True],
    )

    pairs = compute_half_neighbor_pairs_with_ase(atoms, args.cutoff)

    write(args.frame_out, atoms, format="extxyz")
    write_edge_csv(args.edges_out, pairs, args.cutoff)

    print(f"Replaced validation frame with {args.frame_out}")
    print(f"Replaced validation ground truth with {len(pairs)} edges in {args.edges_out}")


if __name__ == "__main__":
    main()
