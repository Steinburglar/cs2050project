#!/bin/bash
#SBATCH --job-name=cs2050-openmp
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=16
#SBATCH --time=00:30:00
#SBATCH --partition=standard
#SBATCH --output=slurm-openmp-%j.out

# Example Slurm run script to execute the OpenMP binary with varying thread counts.
# Edit `--partition`, `--time`, and `--cpus-per-task` to match your cluster.

set -euo pipefail

THREADS_LIST=(1 2 4 8 16)

# Determine repo root from submission directory (assumes you `sbatch` from repo root)
REPO_DIR=${SLURM_SUBMIT_DIR:-$(pwd)}
OPENMP_BUILD_DIR="$REPO_DIR/openmp/build"
OPENMP_EXEC="$OPENMP_BUILD_DIR/openmp_exec"
INPUT_CSV="$REPO_DIR/serial/tests/sample_frame.csv"
RESULTS_DIR="$REPO_DIR/openmp/results"

mkdir -p "$RESULTS_DIR"

if [ ! -x "$OPENMP_EXEC" ]; then
  echo "Executable not found: $OPENMP_EXEC"
  echo "Build it first (from repo root): mkdir -p openmp/build && cd openmp/build && cmake .. && cmake --build ."
  exit 1
fi

echo "Running OpenMP benchmarks on $(hostname)"
for t in "${THREADS_LIST[@]}"; do
  export OMP_NUM_THREADS=$t
  out="$RESULTS_DIR/run_t${t}.txt"
  echo "--- RUN: OMP_NUM_THREADS=$t  ($(date)) ---" | tee "$out"
  # Use srun to launch the process under Slurm's allocator
  start=$(date +%s)
  srun --cpu-bind=cores --ntasks=1 --cpus-per-task=$t "$OPENMP_EXEC" "$INPUT_CSV" >> "$out" 2>&1
  end=$(date +%s)
  echo "completed: $t threads" | tee -a "$out"
  echo "Elapsed: $((end-start)) seconds" | tee -a "$out"
done

echo "All runs complete. Results in $RESULTS_DIR"
