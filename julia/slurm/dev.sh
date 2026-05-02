#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
INPUT="$ROOT_DIR/validation/validation_frame.xyz"
OUTPUT="$ROOT_DIR/julia/results/dev_edges.csv"

mkdir -p "$ROOT_DIR/julia/results"
julia "$ROOT_DIR/julia/src/main.jl" "$INPUT" "$OUTPUT" 5.0 --no-write
