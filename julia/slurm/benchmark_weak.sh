#!/bin/bash
#SBATCH --job-name=cs2050_julia_weak
#SBATCH --output=benchmark_weak.out
#SBATCH --error=benchmark_weak.err
#SBATCH --time=00:20:00
#SBATCH --cpus-per-task=16
#SBATCH --mem=4G

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
RESULT_DIR="$ROOT_DIR/julia/results"
SERIES=(
    "$ROOT_DIR/benchmark/bench_1000.xyz"
    "$ROOT_DIR/benchmark/bench_2000.xyz"
    "$ROOT_DIR/benchmark/bench_4000.xyz"
    "$ROOT_DIR/benchmark/bench_8000.xyz"
    "$ROOT_DIR/benchmark/bench_16000.xyz"
)
THREADS_LIST=(1 2 4 8 16)
OUT_CSV="$RESULT_DIR/julia_weak.csv"

mkdir -p "$RESULT_DIR"

if [[ ! -f "${SERIES[0]}" ]]; then
    "$ROOT_DIR/benchmark/generate_benchmark_series.sh" \
        --base-n 100 \
        --base-box 50.0 \
        --start-n 1000 \
        --factor 2 \
        --count 5 \
        --seed 42 \
        --out-dir "$ROOT_DIR/benchmark"
fi

run_exec() {
    if command -v srun >/dev/null 2>&1 && [[ -n "${SLURM_JOB_ID:-}" ]]; then
        srun --exclusive env JULIA_NUM_THREADS="$1" julia "$ROOT_DIR/julia/src/main.jl" "${@:2}"
    else
        env JULIA_NUM_THREADS="$1" julia "$ROOT_DIR/julia/src/main.jl" "${@:2}"
    fi
}

printf "nthreads,N,load_ms,build_ms,write_ms,total_ms\n" > "$OUT_CSV"

for idx in "${!SERIES[@]}"; do
    input="${SERIES[$idx]}"
    t="${THREADS_LIST[$idx]}"
    if [[ ! -f "$input" ]]; then
        echo "Missing benchmark frame: $input" >&2
        exit 1
    fi
    echo "Running threads=$t N=$(sed -n '1p' "$input")"
    timing_line=$(run_exec "$t" "$input" "$RESULT_DIR/julia_weak.csv.tmp" 10.0 --timing --no-write | awk '/Timing summary/ {print}')
    load_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*load=\([^ ]*\).*/\1/p')
    build_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*build=\([^ ]*\).*/\1/p')
    write_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*write=\([^ ]*\).*/\1/p')
    total_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*total=\([^ ]*\).*/\1/p')
    n=$(sed -n '1p' "$input")
    printf "%s,%s,%s,%s,%s,%s\n" "$t" "$n" "$load_ms" "$build_ms" "$write_ms" "$total_ms" >> "$OUT_CSV"
done

echo "Wrote $OUT_CSV"
