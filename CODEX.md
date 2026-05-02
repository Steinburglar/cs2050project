# Project context and agent notes

## Status
- Repo initialized as git repo.
- Scaffolding in place: `README.md`, `report/report.md`, `CODEX.md`, placeholder folders for serial/openmp/mpi/cuda/julia.

## Goals
- Implement and compare serial, OpenMP, MPI, CUDA, plus one extra method.
- Use Slurm scripts for reproducible runs, timings, results, plots.
- Write final report on methods, performance, conclusions.

## Project choice
Task: molecular-dynamics neighbor construction.
- Input: 3D point cloud.
- Output: half neighbor list, pairs with distance `<= cutoff`.
- Why: meaningful compute, easy to test, supports threads, domain decomposition, GPU kernels.

## Current plan
1. Serial brute-force baseline.
2. OpenMP shared-memory version.
3. MPI domain decomposition + halo exchange.
4. CUDA brute-force baseline.
5. Julia extra method.

## Input format
- Primary input: single-frame `extxyz`.
- Atom line: `symbol x y z`.
- Metadata: `Lattice=` box, `pbc=`, optional `frame_label`.
- Use simple orthogonal box first.

## Shared data model
### `Frame`
- Holds coordinates, optional species, box, periodicity, label.
- Host coords are AoS: `std::vector<std::array<double, 3>>`.
- Fine for serial/OpenMP/MPI.
- CUDA repacks host AoS to temporary SoA: `x[]`, `y[]`, `z[]`.

### `EdgeList`
- SoA output:
  - `sources[]`
  - `destinations[]`
- Canonical output for validation and gathering.

## Output semantics
- Half-list by default.
- 0-based atom indices.
- Store each pair once.
- Use `i < j`.
- CSV output: `source,destination`.

## Validation / benchmark
### Validation
- Shared fixtures in `validation/`.
- Deterministic `validation_frame.xyz`.
- Ground truth `validation_expected_edges.csv`.
- Compare exact edge-list equality after deterministic sort/order.

### Benchmark
- Larger fixed-seed systems in `benchmark/`.
- Stable inputs for repeated timing runs.

### Profiling
- Measure load, pack, upload, build, write, total.
- Tools:
  - `std::chrono`
  - VTune for CPU
  - Nsight Systems / Nsight Compute for CUDA

## Serial
- Correctness baseline.
- Load frame.
- For each `i`, test `j > i`.
- Compute minimum-image squared distance.
- Append `(i, j)` if `dist2 <= cutoff2`.
- Deterministic order from increasing `i`, then `j`.

## OpenMP
- Keep same global brute-force task as serial.
- Parallelize outer loop over `i`.
- Give each source atom local container.
- Merge containers in increasing `i` order.
- No hot-loop `critical`.

## MPI
- MPI changes structure: distributed memory needs domain decomposition + halo exchange.
- Use 3D Cartesian grid from `MPI_Dims_create` + `MPI_Cart_create`.
- Each atom has one global ID and one owner rank.
- Each rank stores:
  - `owned_atoms`
  - `halo_atoms`
- Owned atoms only are sources.
- Halo atoms are destination-only context.
- Rank 0 loads frame, broadcasts box + periodicity, distributes owned atoms.
- Halo exchange:
  - `x`
  - `y`
  - `z`
  - skip axis if `dims[axis] == 1`
  - if low and high neighbors are same rank, send union once
- Assumption: `cutoff <= local subdomain width` in each decomposed axis.
- Validation mode:
  - gather all local edge lists to rank 0
  - sort by `(source, destination)`
  - write CSV
- Benchmark mode:
  - do not gather full edges
  - reduce summary data only

## CUDA
- First CUDA version stays on same global brute-force task as serial/OpenMP.
- Host:
  - load `extxyz`
  - pack AoS -> SoA
  - allocate device buffers
  - copy coords + metadata to GPU
  - launch kernels
  - copy results back only if needed
- Device:
  - evaluate candidate pairs
  - apply minimum-image distance
  - fill output arrays
- Baseline kernel:
  - one thread per source atom `i`
  - scan all `j > i`
- Output strategies:
  - atomic append: simpler, more contention
  - two-pass count/fill: cleaner, more moving parts
- Current choice: two-pass count/fill.

## Julia
- Extra method for project.
- Good for scientific array code, development speed, and comparison vs C++.
- Same input, cutoff, output, validation file as other baselines.
- Start with brute-force script, then add threads if wanted.

## Possible later optimizations
- Cell lists for OpenMP.
- Cell lists for CUDA.
- More advanced CUDA output strategies.
- More optimized MPI packing.

## Notes
- Keep baseline comparison fair: serial, OpenMP, CUDA stay brute-force.
- MPI is different by nature because distributed memory forces decomposition.
- If cell lists are added later, treat them as explicit optimization layer, not hidden baseline change.
