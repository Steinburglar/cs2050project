#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
BUILD_DIR="$ROOT_DIR/mpi/build"
INPUT="$ROOT_DIR/validation/validation_frame.xyz"

export OMP_NUM_THREADS=1

cmake -S "$ROOT_DIR/mpi" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR"

mpirun -n 4 "$BUILD_DIR/mpi_exec" "$INPUT" - 5.0 --no-write
