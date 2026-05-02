#!/bin/bash
#SBATCH --job-name=cs2050_benchmark
#SBATCH --output=benchmark.out
#SBATCH --error=benchmark.err
#SBATCH --time=04:00:00
#SBATCH --cpus-per-task=1
#SBATCH --mem=8gb

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

run_exec() {
    if command -v srun >/dev/null 2>&1 && [[ -n "${SLURM_JOB_ID:-}" ]]; then
        srun --exclusive "$@"
    else
        "$@"
    fi
}

printf "N,load_ms,build_ms,write_ms,total_ms\n" > "$OUT_CSV"

for input in "${SERIES[@]}"; do
    if [[ ! -f "$input" ]]; then
        echo "Missing benchmark frame: $input" >&2
        exit 1
    fi
    n=$(basename "$input" .xyz | sed 's/bench_//')
    echo "Running N=$n"
    timing_line=$(run_exec "$BUILD_DIR/serial_exec" "$input" - 10.0 --timing --no-write | awk '/Timing summary/ {print}')
    load_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*load=\([^ ]*\).*/\1/p')
    build_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*build=\([^ ]*\).*/\1/p')
    write_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*write=\([^ ]*\).*/\1/p')
    total_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*total=\([^ ]*\).*/\1/p')
    printf "%s,%s,%s,%s,%s\n" "$n" "$load_ms" "$build_ms" "$write_ms" "$total_ms" >> "$OUT_CSV"
done

echo "Wrote $OUT_CSV"
