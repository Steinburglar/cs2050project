#!/bin/bash
#SBATCH --job-name=cs2050_mpi_weak
#SBATCH --output=benchmark_weak.out
#SBATCH --error=benchmark_weak.err
#SBATCH --time=00:20:00
#SBATCH --ntasks=16
#SBATCH --cpus-per-task=1
#SBATCH --mem=4G

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
BUILD_DIR="$ROOT_DIR/mpi/build"
RESULT_DIR="$ROOT_DIR/mpi/results"
SERIES=(
    "$ROOT_DIR/benchmark/bench_1000.xyz"
    "$ROOT_DIR/benchmark/bench_2000.xyz"
    "$ROOT_DIR/benchmark/bench_4000.xyz"
    "$ROOT_DIR/benchmark/bench_8000.xyz"
    "$ROOT_DIR/benchmark/bench_16000.xyz"
)
RANKS_LIST=(1 2 4 8 16)
OUT_CSV="$RESULT_DIR/mpi_weak.csv"

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

cmake -S "$ROOT_DIR/mpi" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

printf "nranks,N,load_ms,build_ms,write_ms,total_ms\n" > "$OUT_CSV"

for idx in "${!SERIES[@]}"; do
    input="${SERIES[$idx]}"
    r="${RANKS_LIST[$idx]}"
    if [[ ! -f "$input" ]]; then
        echo "Missing benchmark frame: $input" >&2
        exit 1
    fi
    echo "Running ranks=$r N=$(sed -n '1p' "$input")"
    timing_line=$(mpirun -n "$r" "$BUILD_DIR/mpi_exec" "$input" - 10.0 --timing --no-write | awk '/Timing summary/ {print}')
    load_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*load=\([^ ]*\).*/\1/p')
    build_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*build=\([^ ]*\).*/\1/p')
    write_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*write=\([^ ]*\).*/\1/p')
    total_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*total=\([^ ]*\).*/\1/p')
    n=$(sed -n '1p' "$input")
    printf "%s,%s,%s,%s,%s,%s\n" "$r" "$n" "$load_ms" "$build_ms" "$write_ms" "$total_ms" >> "$OUT_CSV"
done

echo "Wrote $OUT_CSV"
