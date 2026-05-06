#!/bin/bash
#SBATCH --job-name=cs2050_mpi_submit
#SBATCH --nodes=1
#SBATCH --ntasks=4
#SBATCH --cpus-per-task=1
#SBATCH --time=00:15:00
#SBATCH --mem=2G
#SBATCH --output=submit-%j.out
#SBATCH --error=submit-%j.err

set -euo pipefail

# Cluster-specific setup hook:
# module load <mpi-module>
# source /path/to/env/init.sh

# Edit this if your cluster wants a different project root discovery.
ROOT_DIR=${SLURM_SUBMIT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}
BUILD_DIR="$ROOT_DIR/mpi/build"
RESULT_DIR="$ROOT_DIR/mpi/results"
INPUT="$ROOT_DIR/validation/validation_frame.xyz"
OUTPUT="$RESULT_DIR/mpi_submit_edges.csv"
CUTOFF="${CUTOFF:-5.0}"
NPROCS="${NPROCS:-4}"

mkdir -p "$RESULT_DIR"

cmake -S "$ROOT_DIR/mpi" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

srun -n "$NPROCS" "$BUILD_DIR/mpi_exec" "$INPUT" "$OUTPUT" "$CUTOFF" --gather-write

echo "First 10 edge pairs:"
head -n 10 "$OUTPUT"
