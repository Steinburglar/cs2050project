#!/bin/bash
#SBATCH --job-name=cs2050_mpi_validate
#SBATCH --output=validate.out
#SBATCH --error=validate.err
#SBATCH --time=00:10:00
#SBATCH --ntasks=4
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
BUILD_DIR="$ROOT_DIR/mpi/build"
INPUT="$ROOT_DIR/validation/validation_frame.xyz"
EXPECTED="$ROOT_DIR/validation/validation_expected_edges.csv"
ACTUAL="$ROOT_DIR/validation/validation_actual_edges_mpi.csv"

export OMP_NUM_THREADS=8

cmake -S "$ROOT_DIR/mpi" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

mpirun -n 4 "$BUILD_DIR/mpi_exec" "$INPUT" "$ACTUAL" 5.0 --gather-write
python3 "$ROOT_DIR/validation/check_edges.py" "$EXPECTED" "$ACTUAL"
