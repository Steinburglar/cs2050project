#!/bin/bash
#SBATCH --job-name=cs2050_mpi_strong
#SBATCH --output=benchmark_strong.out
#SBATCH --error=benchmark_strong.err
#SBATCH --time=00:20:00
#SBATCH --ntasks=16
#SBATCH --cpus-per-task=1
#SBATCH --mem=4G

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
BUILD_DIR="$ROOT_DIR/mpi/build"
RESULT_DIR="$ROOT_DIR/mpi/results"
INPUT="$ROOT_DIR/benchmark/bench_100000.xyz"
OUT_CSV="$RESULT_DIR/mpi_strong.csv"
RANKS_LIST=(1 2 4 8 16)

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

cmake -S "$ROOT_DIR/mpi" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

printf "nranks,N,load_ms,build_ms,write_ms,total_ms\n" > "$OUT_CSV"

for r in "${RANKS_LIST[@]}"; do
    if [[ -n "${SLURM_NTASKS:-}" && "$r" -gt "$SLURM_NTASKS" ]]; then
        continue
    fi
    echo "Running ranks=$r"
    timing_line=$(mpirun -n "$r" "$BUILD_DIR/mpi_exec" "$INPUT" - 10.0 --timing --no-write | awk '/Timing summary/ {print}')
    load_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*load=\([^ ]*\).*/\1/p')
    build_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*build=\([^ ]*\).*/\1/p')
    write_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*write=\([^ ]*\).*/\1/p')
    total_ms=$(printf '%s\n' "$timing_line" | sed -n 's/.*total=\([^ ]*\).*/\1/p')
    n=$(sed -n '1p' "$INPUT")
    printf "%s,%s,%s,%s,%s,%s\n" "$r" "$n" "$load_ms" "$build_ms" "$write_ms" "$total_ms" >> "$OUT_CSV"
done

echo "Wrote $OUT_CSV"
