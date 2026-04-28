#!/bin/bash
#SBATCH --job-name=cs2050_benchmark
#SBATCH --output=benchmark.out
#SBATCH --error=benchmark.err
#SBATCH --time=00:10:00
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
BUILD_DIR="$ROOT_DIR/serial/build"
INPUT="$ROOT_DIR/benchmark/benchmark.xyz"
OUTPUT="/dev/null"

cmake -S "$ROOT_DIR/serial" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

"$BUILD_DIR/serial_exec" "$INPUT" "$OUTPUT" 50.0 50.0 50.0 T T T 10.0 --timing | grep -E '^(Found |Timing summary)'
