#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

bash "$ROOT_DIR/serial/slurm/validate.sh"
bash "$ROOT_DIR/openmp/slurm/validation.sh"
bash "$ROOT_DIR/mpi/slurm/validation.sh"
bash "$ROOT_DIR/cuda/slurm/validation.sh"
bash "$ROOT_DIR/julia/slurm/validation.sh"
