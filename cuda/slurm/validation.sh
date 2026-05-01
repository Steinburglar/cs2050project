#!/bin/bash
#SBATCH --job-name=cs2050_cuda_validate
#SBATCH --output=validate.out
#SBATCH --error=validate.err
#SBATCH --time=00:10:00
#SBATCH --cpus-per-task=1
#SBATCH --gres=gpu:1
#SBATCH --mem=2G

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
BUILD_DIR="$ROOT_DIR/cuda/build"
INPUT="$ROOT_DIR/validation/validation_frame.xyz"
EXPECTED="$ROOT_DIR/validation/validation_expected_edges.csv"
ACTUAL="$ROOT_DIR/validation/validation_actual_edges_cuda.csv"

cmake -S "$ROOT_DIR/cuda" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

"$BUILD_DIR/cuda_exec" "$INPUT" "$ACTUAL" 5.0
python3 "$ROOT_DIR/validation/check_edges.py" "$EXPECTED" "$ACTUAL"
