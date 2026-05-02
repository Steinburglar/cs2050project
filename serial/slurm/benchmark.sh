#!/bin/bash
#SBATCH --job-name=cs2050_benchmark
#SBATCH --output=benchmark.out
#SBATCH --error=benchmark.err
#SBATCH --time=00:20:00
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
BUILD_DIR="$ROOT_DIR/serial/build"
RESULT_DIR="$ROOT_DIR/serial/results"
SERIES=(
    "$ROOT_DIR/benchmark/bench_100.xyz"
    "$ROOT_DIR/benchmark/bench_1000.xyz"
    "$ROOT_DIR/benchmark/bench_10000.xyz"
    "$ROOT_DIR/benchmark/bench_100000.xyz"
    "$ROOT_DIR/benchmark/bench_1000000.xyz"
)
OUT_CSV="$RESULT_DIR/serial_scaling.csv"

mkdir -p "$RESULT_DIR"

cmake -S "$ROOT_DIR/serial" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

printf "N,total_ms\n" > "$OUT_CSV"

for input in "${SERIES[@]}"; do
    n=$(basename "$input" .xyz | sed 's/bench_//')
    echo "Running N=$n"
    line=$("$BUILD_DIR/serial_exec" "$input" - 10.0 --timing --no-write | awk -F'total=' '/Timing summary/ {print $2}')
    printf "%s,%s\n" "$n" "$line" >> "$OUT_CSV"
done

echo "Wrote $OUT_CSV"
