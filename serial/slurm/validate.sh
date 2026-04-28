#!/bin/bash
#SBATCH --job-name=cs2050_validate
#SBATCH --output=validate.out
#SBATCH --error=validate.err
#SBATCH --time=00:10:00
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
BUILD_DIR="$ROOT_DIR/serial/build"
INPUT="$ROOT_DIR/validation/validation_frame.xyz"
EXPECTED="$ROOT_DIR/validation/validation_expected_edges.csv"
ACTUAL="$ROOT_DIR/validation/validation_actual_edges.csv"

cmake -S "$ROOT_DIR/serial" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

"$BUILD_DIR/serial_exec" "$INPUT" "$ACTUAL" 10.0 10.0 10.0 T T T 1.5
python3 "$ROOT_DIR/validation/check_edges.py" "$EXPECTED" "$ACTUAL"
