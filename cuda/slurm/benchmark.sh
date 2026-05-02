#!/bin/bash
#SBATCH --job-name=cs2050_cuda_bench
#SBATCH --output=benchmark.out
#SBATCH --error=benchmark.err
#SBATCH --time=00:30:00
#SBATCH --cpus-per-task=1
#SBATCH --gres=gpu:1
#SBATCH --mem=4G

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
BUILD_DIR="$ROOT_DIR/cuda/build"
RESULT_DIR="$ROOT_DIR/cuda/results"
OUT_CSV="$RESULT_DIR/cuda_problem_size.csv"
SERIES=(
    "$ROOT_DIR/benchmark/bench_100.xyz"
    "$ROOT_DIR/benchmark/bench_1000.xyz"
    "$ROOT_DIR/benchmark/bench_10000.xyz"
    "$ROOT_DIR/benchmark/bench_100000.xyz"
    "$ROOT_DIR/benchmark/bench_1000000.xyz"
)

mkdir -p "$RESULT_DIR"

if [[ ! -f "${SERIES[-1]}" ]]; then
    "$ROOT_DIR/benchmark/generate_benchmark_series.sh" \
        --base-n 100 \
        --base-box 50.0 \
        --start-n 100 \
        --factor 10 \
        --count 5 \
        --seed 42 \
        --out-dir "$ROOT_DIR/benchmark"
fi

cmake -S "$ROOT_DIR/cuda" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

run_exec() {
    if command -v srun >/dev/null 2>&1 && [[ -n "${SLURM_JOB_ID:-}" ]]; then
        srun --exclusive "$@"
    else
        "$@"
    fi
}

printf "N,load_ms,pack_ms,upload_ms,build_ms,write_ms,total_ms\n" > "$OUT_CSV"

for input in "${SERIES[@]}"; do
    if [[ ! -f "$input" ]]; then
        echo "Missing benchmark frame: $input" >&2
        exit 1
    fi
    n=$(basename "$input" .xyz | sed 's/bench_//')
    echo "Running N=$n"
    timing_line=$(run_exec "$BUILD_DIR/cuda_exec" "$input" - 10.0 --timing --no-write | awk '/Timing summary/ {print}')
    load_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*load=\([^ ]*\).*/\1/p')
    pack_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*pack=\([^ ]*\).*/\1/p')
    upload_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*upload=\([^ ]*\).*/\1/p')
    build_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*build=\([^ ]*\).*/\1/p')
    write_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*write=\([^ ]*\).*/\1/p')
    total_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*total=\([^ ]*\).*/\1/p')
    printf "%s,%s,%s,%s,%s,%s,%s\n" "$n" "$load_ms" "$pack_ms" "$upload_ms" "$build_ms" "$write_ms" "$total_ms" >> "$OUT_CSV"
done

echo "Wrote $OUT_CSV"
