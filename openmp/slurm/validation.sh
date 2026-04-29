#!/bin/bash
#SBATCH --job-name=cs2050_omp_validate
#SBATCH --output=validate.out
#SBATCH --error=validate.err
#SBATCH --time=00:10:00
#SBATCH --cpus-per-task=4
#SBATCH --mem=2G

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
BUILD_DIR="$ROOT_DIR/openmp/build"
INPUT="$ROOT_DIR/validation/validation_frame.xyz"
EXPECTED="$ROOT_DIR/validation/validation_expected_edges.csv"
ACTUAL="$ROOT_DIR/validation/validation_actual_edges_openmp.csv"

export OMP_NUM_THREADS=4

cmake -S "$ROOT_DIR/openmp" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

"$BUILD_DIR/openmp_exec" "$INPUT" "$ACTUAL" 1.5
python3 "$ROOT_DIR/validation/check_edges.py" "$EXPECTED" "$ACTUAL"
