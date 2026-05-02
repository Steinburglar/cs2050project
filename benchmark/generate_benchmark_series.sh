#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
GEN="$ROOT_DIR/benchmark/generate_benchmark_frame.py"
OUT_DIR="$ROOT_DIR/benchmark"

N_START=100
BOX_START=50.0
SEED=42
COUNT=5
SCALE=$(python3 - <<'PY'
import math
print(f"{10 ** (1/3):.12f}")
PY
)

mkdir -p "$OUT_DIR"

n=$N_START
box=$BOX_START
for _ in $(seq 1 "$COUNT"); do
    out="$OUT_DIR/bench_${n}.xyz"
    python3 "$GEN" --n "$n" --box "$box" --seed "$SEED" --out "$out"
    n=$((n * 10))
    box=$(python3 - <<PY
box = float("$box")
scale = float("$SCALE")
print(f"{box * scale:.12f}")
PY
)
done
