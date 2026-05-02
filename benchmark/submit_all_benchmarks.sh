#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

JOBS=(
    "$ROOT_DIR/serial/slurm/benchmark.sh"
    "$ROOT_DIR/openmp/slurm/benchmark_strong.sh"
    "$ROOT_DIR/openmp/slurm/benchmark_weak.sh"
    "$ROOT_DIR/mpi/slurm/benchmark_strong.sh"
    "$ROOT_DIR/mpi/slurm/benchmark_weak.sh"
    "$ROOT_DIR/cuda/slurm/benchmark.sh"
    "$ROOT_DIR/julia/slurm/benchmark_strong.sh"
    "$ROOT_DIR/julia/slurm/benchmark_weak.sh"
)

if ! command -v sbatch >/dev/null 2>&1; then
    echo "sbatch not found. Run this on a Slurm login node." >&2
    exit 1
fi

for job in "${JOBS[@]}"; do
    if [[ ! -f "$job" ]]; then
        echo "Missing benchmark script: $job" >&2
        exit 1
    fi
done

echo "Submitting benchmark jobs..."
for job in "${JOBS[@]}"; do
    job_id=$(sbatch "$job" | awk '{print $4}')
    echo "$job -> $job_id"
done
