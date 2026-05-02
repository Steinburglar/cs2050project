#!/bin/bash
#SBATCH --job-name=cs2050_omp_strong
#SBATCH --output=benchmark_strong.out
#SBATCH --error=benchmark_strong.err
#SBATCH --time=00:20:00
#SBATCH --cpus-per-task=16
#SBATCH --mem=4G

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
BUILD_DIR="$ROOT_DIR/openmp/build"
RESULT_DIR="$ROOT_DIR/openmp/results"
INPUT="$ROOT_DIR/benchmark/bench_100000.xyz"
OUT_CSV="$RESULT_DIR/openmp_strong.csv"
THREADS_LIST=(1 2 4 8 16)

mkdir -p "$RESULT_DIR"

if [[ ! -f "$INPUT" ]]; then
    "$ROOT_DIR/benchmark/generate_benchmark_series.sh" \
        --base-n 100 \
        --base-box 50.0 \
        --start-n 100000 \
        --factor 1 \
        --count 1 \
        --seed 42 \
        --out-dir "$ROOT_DIR/benchmark"
fi

cmake -S "$ROOT_DIR/openmp" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

run_exec() {
    if command -v srun >/dev/null 2>&1 && [[ -n "${SLURM_JOB_ID:-}" ]]; then
        srun --exclusive env OMP_NUM_THREADS="$1" "${@:2}"
    else
        env OMP_NUM_THREADS="$1" "${@:2}"
    fi
}

printf "nthreads,N,load_ms,build_ms,write_ms,total_ms\n" > "$OUT_CSV"

for t in "${THREADS_LIST[@]}"; do
    if [[ -n "${SLURM_CPUS_PER_TASK:-}" && "$t" -gt "$SLURM_CPUS_PER_TASK" ]]; then
        continue
    fi
    echo "Running threads=$t"
    timing_line=$(run_exec "$t" "$BUILD_DIR/openmp_exec" "$INPUT" - 10.0 --timing --no-write | awk '/Timing summary/ {print}')
    load_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*load=\([^ ]*\).*/\1/p')
    build_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*build=\([^ ]*\).*/\1/p')
    write_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*write=\([^ ]*\).*/\1/p')
    total_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*total=\([^ ]*\).*/\1/p')
    n=$(sed -n '1p' "$INPUT")
    printf "%s,%s,%s,%s,%s,%s\n" "$t" "$n" "$load_ms" "$build_ms" "$write_ms" "$total_ms" >> "$OUT_CSV"
done

echo "Wrote $OUT_CSV"
