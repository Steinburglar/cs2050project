# Plotting Notes

Use `benchmark/plotting/plot_single_csv.py` to turn one benchmark CSV into one
figure.

## General Form

```bash
python3 benchmark/plotting/plot_single_csv.py \
  <csv-path> \
  --title "<plot title>" \
  --metric time|speedup|efficiency \
  --x-col <x column name> \
  --y-col <time column name> \
  --x-scale lin|log \
  --y-scale lin|log \
  --xlabel "<x label>" \
  --fit-slope \
  --ref-exp <exponent> \
  --ref-label "<ref label>" \
  --out <figure name>
```

Defaults:
- `--metric time`
- `--ylabel` is inferred from `--metric` and `--y-col`
- `--ylabel` can override inferred label, with warning
- figures save to `<csv-dir>/figures/`

## Serial Problem-Size Scaling

Use raw time.

```bash
python3 benchmark/plotting/plot_single_csv.py \
  serial/results/serial_scaling.csv \
  --title "Serial Problem-Size Scaling" \
  --metric time \
  --x-col N \
  --y-col total_ms \
  --x-scale log \
  --y-scale log \
  --xlabel "N" \
  --fit-slope \
  --ref-exp 2 \
  --ref-label "N^2" \
  --out serial_scaling.png
```

For build-only view:

```bash
python3 benchmark/plotting/plot_single_csv.py \
  serial/results/serial_scaling.csv \
  --title "Serial Build Scaling" \
  --metric time \
  --x-col N \
  --y-col build_ms \
  --x-scale log \
  --y-scale log \
  --xlabel "N" \
  --fit-slope \
  --ref-exp 2 \
  --ref-label "N^2" \
  --out serial_build.png
```

## OpenMP Strong Scaling

Use speedup.

```bash
python3 benchmark/plotting/plot_single_csv.py \
  openmp/results/openmp_strong.csv \
  --title "OpenMP Strong Scaling" \
  --metric speedup \
  --x-col nthreads \
  --y-col total_ms \
  --x-scale log \
  --y-scale log \
  --xlabel "Threads" \
  --fit-slope \
  --ref-exp 1 \
  --ref-label "Ideal Speedup" \
  --out openmp_strong.png
```

## OpenMP Weak Scaling

Use efficiency.

```bash
python3 benchmark/plotting/plot_single_csv.py \
  openmp/results/openmp_weak.csv \
  --title "OpenMP Weak Scaling" \
  --metric efficiency \
  --x-col nthreads \
  --y-col total_ms \
  --x-scale log \
  --y-scale log \
  --xlabel "Threads" \
  --fit-slope \
  --ref-exp 0 \
  --ref-label "Ideal Weak Scaling" \
  --out openmp_weak.png
```

## Julia Strong Scaling

```bash
python3 benchmark/plotting/plot_single_csv.py \
  julia/results/julia_strong.csv \
  --title "Julia Strong Scaling" \
  --metric speedup \
  --x-col nthreads \
  --y-col total_ms \
  --x-scale log \
  --y-scale log \
  --xlabel "Threads" \
  --fit-slope \
  --ref-exp 1 \
  --ref-label "Ideal Speedup" \
  --out julia_strong.png
```

## Julia Weak Scaling

```bash
python3 benchmark/plotting/plot_single_csv.py \
  julia/results/julia_weak.csv \
  --title "Julia Weak Scaling" \
  --metric efficiency \
  --x-col nthreads \
  --y-col total_ms \
  --x-scale log \
  --y-scale log \
  --xlabel "Threads" \
  --fit-slope \
  --ref-exp 0 \
  --ref-label "Ideal Weak Scaling" \
  --out julia_weak.png
```

## MPI Strong Scaling

```bash
python3 benchmark/plotting/plot_single_csv.py \
  mpi/results/mpi_strong.csv \
  --title "MPI Strong Scaling" \
  --metric speedup \
  --x-col nranks \
  --y-col total_ms \
  --x-scale log \
  --y-scale log \
  --xlabel "Ranks" \
  --fit-slope \
  --ref-exp 1 \
  --ref-label "Ideal Speedup" \
  --out mpi_strong.png
```

## MPI Weak Scaling

```bash
python3 benchmark/plotting/plot_single_csv.py \
  mpi/results/mpi_weak.csv \
  --title "MPI Weak Scaling" \
  --metric efficiency \
  --x-col nranks \
  --y-col total_ms \
  --x-scale log \
  --y-scale log \
  --xlabel "Ranks" \
  --fit-slope \
  --ref-exp 0 \
  --ref-label "Ideal Weak Scaling" \
  --out mpi_weak.png
```

## CUDA Problem-Size Scaling

Use raw time for the GPU problem-size sweep.

```bash
python3 benchmark/plotting/plot_single_csv.py \
  cuda/results/cuda_problem_size.csv \
  --title "CUDA Problem-Size Scaling" \
  --metric time \
  --x-col N \
  --y-col build_ms \
  --x-scale log \
  --y-scale log \
  --xlabel "N" \
  --fit-slope \
  --ref-exp 2 \
  --ref-label "N^2" \
  --out cuda_problem_size_build.png
```

If you want end-to-end runtime instead, keep the same command and switch
`--y-col total_ms`.

## Cross-Method Problem-Size Scaling

Plot the main build-time comparison across methods on one figure:

```bash
python3 benchmark/plotting/plot_cross_method_problem_size.py \
  --title "Cross-Method Problem-Size Scaling" \
  --metric time \
  --x-scale log \
  --y-scale log \
  --xlabel "N" \
  --out cross_method_problem_size.png
```

The script reads the current method CSVs from:
- `serial/results/serial_scaling.csv`
- `openmp/results/openmp_strong.csv`
- `julia/results/julia_strong.csv`
- `mpi/results/mpi_strong.csv`
- `cuda/results/cuda_problem_size.csv`

## Notes

- CSVs go in `<method>/results/`.
- Figures go in `<method>/results/figures/`.
- Use `--metric` to pick raw time, speedup, or efficiency.
- Use `--x-col` and `--y-col` to pick columns from labeled CSV.
- Use `--fit-slope` to estimate scaling exponent in selected axis scales.
- Use `--ref-exp` and `--ref-label` to add dotted reference lines.
