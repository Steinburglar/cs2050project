# Algorithms and Implementation Plan

This document records the current mid-level design for the project. The goal is
to keep the core task fixed and comparable across implementations while still
using natural parallelization strategies for each paradigm.

## Core Task

- Input: a single-frame `extxyz` file with atom positions, box, and periodicity
- Parameter: cutoff radius `r_c`
- Output: a binary half neighbor list

Neighbor-list semantics:

- store each pair once
- use `i < j`
- include a pair if the minimum-image distance is `<= r_c`
- write output as a two-column CSV: `source,destination`

## Shared Data Model

### Frame

`Frame` stores:

- coordinates
- optional species
- orthogonal box lengths
- periodicity flags

Current host-side coordinate layout is effectively **AoS**:

- `std::vector<std::array<double, 3>>`

That is fine for serial, OpenMP, and MPI. For CUDA, we currently plan to repack
host coordinates into a temporary **SoA** layout before upload:

- `x[]`
- `y[]`
- `z[]`

If profiling later shows that repacking is significant, `Frame` is a natural
place to revisit.

### Edge List

`EdgeList` is a structure-of-arrays container:

- `sources[]`
- `destinations[]`

This makes validation and gathering straightforward.

## Validation, Benchmarking, and Profiling

### Validation

Validation uses shared fixtures in `validation/`:

- deterministic `extxyz` frame
- ground-truth edge CSV

Validation should always check exact edge-list equality after sorting or using
deterministic output order.

### Benchmarking

Benchmarking uses shared fixtures in `benchmark/`:

- larger fixed-seed systems
- stable inputs for repeated timing runs

### Profiling

Measure at least:

- load time
- build time
- write time
- total time

Use:

- built-in `std::chrono` timers for routine measurements
- VTune for CPU analysis
- Nsight Systems / Nsight Compute for CUDA analysis

The validation case is for correctness, not performance.

## Julia Extension

Julia be the extra method for project.

Why Julia:

- easier to write than C++/CUDA
- good for scientific array code
- still serious HPC language
- good comparison point for speed vs development ease

What Julia should do:

1. load same `extxyz`
2. build same half neighbor list
3. use same cutoff and minimum-image rule
4. write same CSV output

Keep it fair:

- same input
- same output
- same validation file
- same cutoff as other baseline runs

Likely Julia shape:

- use `DelimitedFiles` or a small parser for input
- store coordinates in arrays
- run brute-force neighbor loop first
- add `Threads.@threads` later if wanted

For the report, compare Julia against C++ on:

- ease of development
- code size / clarity
- runtime on benchmark case
- abstraction level

Julia be a good extra method because it show different language style
without forcing a big new framework.

## Serial Plan

The serial implementation is the correctness baseline.

Algorithm:

1. Load the `extxyz` frame.
2. For each atom `i`:
   - for each atom `j > i`:
     - compute minimum-image squared distance
     - if `dist2 <= cutoff2`, append `(i, j)`
3. Write the edge list if requested.

Pseudocode:

```text
load Frame
cutoff2 = cutoff * cutoff
for i in 0 .. N-1:
    for j in i+1 .. N-1:
        if distance2(i, j) <= cutoff2:
            append (i, j)
```

Notes:

- deterministic output order comes naturally from increasing `i`, then `j`
- no spatial acceleration structure in the baseline

## OpenMP Plan

The OpenMP version should keep the same global brute-force task as the serial
version.

Algorithm:

1. Load the frame serially.
2. Parallelize the outer loop over source atoms `i`.
3. Give each source atom its own local destination container.
4. Concatenate per-source containers in increasing `i` order.

Pseudocode:

```text
load Frame
cutoff2 = cutoff * cutoff
create per-source destination containers

parallel for i in 0 .. N-1 schedule(dynamic):
    for j in i+1 .. N-1:
        if distance2(i, j) <= cutoff2:
            append j to source i's local container

concatenate source containers in increasing i order
```

Why this design:

- no `critical` section in the hot loop
- deterministic final edge order
- close algorithmic match to the serial baseline

## MPI Plan

MPI is necessarily more algorithmically structured because distributed memory
requires spatial decomposition and halo exchange. So MPI is not a perfectly
identical low-level algorithm to serial/OpenMP, but it solves the same task and
preserves the same edge semantics.

### Domain Decomposition

Use a 3D Cartesian process grid:

- choose dimensions with `MPI_Dims_create`
- create a Cartesian communicator with `MPI_Cart_create`

Each atom has:

- one global atom ID
- exactly one owner rank based on spatial location

### Owned and Halo Atoms

Each rank stores:

- `owned_atoms`
- `halo_atoms`

Each rank emits edges only for owned atoms. Halo atoms are destination-only
context.

### Ownership Distribution

1. Rank 0 loads the frame.
2. Rank 0 broadcasts box and periodic metadata.
3. Rank 0 computes the owner rank for each atom.
4. Rank 0 sends each rank its owned atoms.

### Halo Exchange

Use a 3-phase directional sweep:

1. exchange along `x`
2. exchange along `y`
3. exchange along `z`

In each phase:

- build low-face and high-face boundary buffers
- exchange counts
- exchange atom payloads
- append received atoms to `halo_atoms`

Special cases:

- if `dims[axis] == 1`, skip exchange in that axis
- if low and high neighbors are the same rank (`dims[axis] == 2` periodic
  case), send one union buffer

Important geometric assumption:

- this first MPI design assumes `cutoff <= local subdomain width` in each
  decomposed axis

Otherwise one-hop nearest-neighbor halo exchange is not sufficient.

### Local Edge Construction

Each rank then builds its local edge list:

- source atoms: owned atoms only
- destination atoms: owned atoms with `j > i`, plus all halo atoms
- global half-list rule: emit only if `global_i < global_j`

Pseudocode:

```text
rank 0 loads Frame
broadcast box + periodicity
create Cartesian communicator
distribute owned atoms
exchange halos

for each owned atom i:
    for each owned atom j with j > i:
        if dist2(i, j) <= cutoff2:
            append (global_i, global_j)
    for each halo atom j:
        if global_i < global_j and dist2(i, j) <= cutoff2:
            append (global_i, global_j)
```

### MPI Output Behavior

Two modes:

- validation mode:
  - gather all local edge lists to rank 0
  - globally sort by `(source, destination)`
  - write CSV
- benchmark mode:
  - do not gather full edge lists
  - reduce only summary data such as edge count and timings

## CUDA Plan

For fairness with the serial and OpenMP baselines, the first CUDA version
should also solve the **global brute-force task**, not a cell-list variant.

That gives the cleanest comparison between:

- serial
- OpenMP
- CUDA

MPI still differs structurally because distributed memory forces domain
decomposition.

### Host / Device Split

Host responsibilities:

- load `extxyz`
- pack coordinates from host AoS into host SoA
- allocate device buffers
- copy coordinates and metadata to the GPU
- launch kernels
- copy back results if needed
- optionally write CSV

Device responsibilities:

- evaluate candidate pairs
- apply minimum-image distance
- decide which pairs belong in the neighbor list
- fill device-side output arrays

### Memory Flow

1. CPU loads `Frame`
2. CPU repacks to:
   - `x[]`
   - `y[]`
   - `z[]`
3. CPU allocates device memory
4. CPU copies SoA buffers and box metadata to device
5. GPU builds the neighbor list
6. CPU copies edges back only when needed

We do **not** plan to parse `extxyz` directly on the GPU in the first version.

### CUDA Kernel Baseline

The simplest fair CUDA baseline is:

- one thread owns one source atom `i`
- that thread scans all `j > i`
- apply minimum-image distance
- count or store valid neighbors

This is still brute-force globally:

- overall candidate work is `O(N^2)`

The advantage is that it matches the serial/OpenMP task more closely than a
cell-list CUDA version would.

### CUDA Output Construction

Two reasonable designs:

#### Option 1: Atomic append

Each source-thread:

- scans `j > i`
- atomically reserves output slots for valid pairs

Pros:

- simpler first CUDA version

Cons:

- global atomic contention
- nondeterministic output order

#### Option 2: Two-pass count/fill

Pass 1:

- one thread per source atom
- count valid neighbors of source atom `i`

Then:

- device prefix sum over per-source counts

Pass 2:

- one thread per source atom again
- rescan `j > i`
- write neighbors into that source atom's assigned output range

Pros:

- cleaner per-source layout
- closer in spirit to the OpenMP and MPI sublists
- less global contention

Cons:

- more moving parts
- requires scan/prefix-sum support

For this project, the two-pass design is preferable if time allows.

### CUDA Pseudocode

```text
host loads Frame
host repacks coordinates to SoA
host allocates device buffers
host copies coordinates + box metadata to device

kernel count_neighbors:
    one thread per source atom i
    for j in i+1 .. N-1:
        if dist2(i, j) <= cutoff2:
            neighbor_count[i] += 1

device prefix sum on neighbor_count -> edge_offsets

kernel fill_neighbors:
    one thread per source atom i
    write_ptr = edge_offsets[i]
    for j in i+1 .. N-1:
        if dist2(i, j) <= cutoff2:
            sources[write_ptr] = i
            destinations[write_ptr] = j
            write_ptr += 1
```

## Possible Later Optimizations

We are intentionally keeping the current baseline task global and brute-force
for fairness.

Possible later extensions:

- cell lists for OpenMP
- cell lists for CUDA
- more advanced CUDA output strategies
- possibly more optimized MPI packing

If we add cell lists later, they should be described explicitly as an
**algorithmic optimization layer**, not folded invisibly into one backend's
baseline.
