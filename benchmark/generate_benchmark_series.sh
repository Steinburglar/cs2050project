#!/bin/bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: generate_benchmark_series.sh [--base-n N] [--base-box L] [--start-n N] [--factor F] [--count C] [--seed S] [--out-dir DIR]

Generate benchmark extxyz frames at constant density.
Each frame uses box derived from base density:
box = base-box * (N / base-n)^(1/3)
EOF
}

BASE_N=100
BASE_BOX=50.0
START_N=100
FACTOR=10
COUNT=5
SEED=42
OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --base-n)
            BASE_N="$2"
            shift 2
            ;;
        --base-box)
            BASE_BOX="$2"
            shift 2
            ;;
        --start-n)
            START_N="$2"
            shift 2
            ;;
        --factor)
            FACTOR="$2"
            shift 2
            ;;
        --count)
            COUNT="$2"
            shift 2
            ;;
        --seed)
            SEED="$2"
            shift 2
            ;;
        --out-dir)
            OUT_DIR="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown arg: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
GEN="$ROOT_DIR/benchmark/generate_benchmark_frame.py"

mkdir -p "$OUT_DIR"

n="$START_N"
for _ in $(seq 1 "$COUNT"); do
    box=$(python3 - <<PY
import math
base_n = float("$BASE_N")
base_box = float("$BASE_BOX")
n = float("$n")
print(f"{base_box * (n / base_n) ** (1/3):.12f}")
PY
)
    out="$OUT_DIR/bench_${n}.xyz"
    python3 "$GEN" --n "$n" --box "$box" --seed "$SEED" --out "$out"
    echo "Wrote $out"
    n=$(python3 - <<PY
n = int("$n")
factor = float("$FACTOR")
print(int(round(n * factor)))
PY
)
done
