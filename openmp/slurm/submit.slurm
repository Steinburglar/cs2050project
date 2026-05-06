#!/bin/bash
#SBATCH --job-name=cs2050_openmp_submit
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --time=00:10:00
#SBATCH --mem=2G
#SBATCH --output=submit-%j.out
#SBATCH --error=submit-%j.err

set -euo pipefail

# Edit this if you submit from outside repo root.
ROOT_DIR=${SLURM_SUBMIT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}
BUILD_DIR="$ROOT_DIR/openmp/build"
RESULT_DIR="$ROOT_DIR/openmp/results"
INPUT="$ROOT_DIR/validation/validation_frame.xyz"
OUTPUT="$RESULT_DIR/openmp_submit_edges.csv"
CUTOFF="${CUTOFF:-5.0}"
THREADS="${OMP_THREADS:-8}"

mkdir -p "$RESULT_DIR"

export OMP_NUM_THREADS="$THREADS"

cmake -S "$ROOT_DIR/openmp" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

"$BUILD_DIR/openmp_exec" "$INPUT" "$OUTPUT" "$CUTOFF"

echo "First 10 edge pairs:"
head -n 10 "$OUTPUT"
