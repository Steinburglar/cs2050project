#!/bin/bash
#SBATCH --job-name=cs2050_julia_validate
#SBATCH --output=validate.out
#SBATCH --error=validate.err
#SBATCH --time=00:10:00
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
INPUT="$ROOT_DIR/validation/validation_frame.xyz"
EXPECTED="$ROOT_DIR/validation/validation_expected_edges.csv"
ACTUAL="$ROOT_DIR/validation/validation_actual_edges_julia.csv"

julia --threads 4 "$ROOT_DIR/julia/src/main.jl" "$INPUT" "$ACTUAL" 5.0 --timing
python3 "$ROOT_DIR/validation/check_edges.py" "$EXPECTED" "$ACTUAL"
