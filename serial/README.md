# Serial Neighbor-List Builder

This directory contains the serial baseline for the project.

## What It Does

The serial executable:

- reads a single-frame `extxyz` file
- loads the periodic box from the `Lattice=` metadata in the comment line
- builds a binary half neighbor list using the cutoff-based minimum-image distance
- writes the result to a two-column CSV file
- can optionally print a lightweight timing summary for load, build, write, and total phases when run with `--timing`

## Build

From this directory:

```bash
cmake -S . -B build
cmake --build build -j
```

## Run

The executable requires explicit arguments:

```bash
./build/serial_exec <input.extxyz> <output.csv|-> <cutoff> [--timing] [--no-write]
```

Example:

```bash
./build/serial_exec ../validation/validation_frame.xyz ../validation/validation_actual_edges.csv 1.5
```

Argument meaning:

- `<input.extxyz>`: input frame file
- `<output.csv|->`: output neighbor list path, or `-` when output is disabled
- `<cutoff>`: neighbor cutoff distance
- `--timing`: optional flag to print the phase timing summary at the end
- `--no-write`: skip the write phase entirely

## Validation

The validation workflow is driven by `serial/slurm/validate.sh`.

It:

1. builds the serial executable
2. runs it on `validation/validation_frame.xyz`
3. compares the produced CSV against `validation/validation_expected_edges.csv`

The comparison is done by `validation/check_edges.py`.

## Benchmarking

The benchmark workflow is driven by `serial/slurm/benchmark.sh`.

It:

1. builds the serial executable in the normal `serial/build/` directory
2. runs it on `benchmark/benchmark.xyz`
3. disables edge writing entirely
4. prints the edge count and timing summary

Both Slurm scripts intentionally use the same build tree so they run the exact same executable; only the runtime arguments differ.

## Notes

- The code currently assumes an orthogonal box.
- The validation frame is deliberately small so it is easy to inspect by hand.
- Benchmark inputs should be generated with `benchmark/generate_benchmark_frame.py`.
- Add `--timing` to a benchmark run if you want the built-in phase summary printed at the end.
