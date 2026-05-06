#!/bin/bash
#SBATCH --job-name=cs2050_cuda_submit
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --gres=gpu:1
#SBATCH --time=00:10:00
#SBATCH --mem=4G
#SBATCH --output=submit-%j.out
#SBATCH --error=submit-%j.err

set -euo pipefail

# Cluster-specific setup hook:
# module load <cuda-module>
# source /path/to/env/init.sh

# Edit this if your cluster wants a different project root discovery.
ROOT_DIR=${SLURM_SUBMIT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}
BUILD_DIR="$ROOT_DIR/cuda/build"
RESULT_DIR="$ROOT_DIR/cuda/results"
INPUT="$ROOT_DIR/validation/validation_frame.xyz"
OUTPUT="$RESULT_DIR/cuda_submit_edges.csv"
CUTOFF="${CUTOFF:-5.0}"

mkdir -p "$RESULT_DIR"

cmake -S "$ROOT_DIR/cuda" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

"$BUILD_DIR/cuda_exec" "$INPUT" "$OUTPUT" "$CUTOFF"

echo "First 10 edge pairs:"
head -n 10 "$OUTPUT"
