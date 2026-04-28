# Algorithms and Implementation Plan

This document records the mid-level design for the serial and OpenMP versions of the project.

The goal is to get the core algorithm and data flow right before we build anything parallel on top of it.

## Problem Definition

- Input: an `extxyz` file containing a 3D point cloud of atoms plus periodic box metadata.
- Parameter: a cutoff radius.
- Output: a binary half neighbor list.

Neighbor-list semantics:

- store each atom pair once
- use `i < j` to enforce the half-list convention
- include a pair if its minimum-image distance is less than or equal to the cutoff
- keep the output binary, not weighted

## Serial Baseline

The serial version is the reference implementation for correctness and timing.

### High-Level Steps

1. Load the `xyz` frame into memory.
2. Attach box and periodicity metadata.
3. Loop over each atom `i`.
4. Loop over each later atom `j > i`.
5. Compute the squared minimum-image distance between `i` and `j`.
6. If `distance^2 <= cutoff^2`, append `(i, j)` to the edge list.
7. Write the half neighbor list to a two-column CSV file.
8. Record timing information for the build step.

### Serial Pseudocode

```text
load Frame
initialize empty EdgeList
cutoff2 = cutoff * cutoff

for i in 0 .. N-1:
    for j in i+1 .. N-1:
        dist2 = frame.distance2(i, j)
        if dist2 <= cutoff2:
            append i to sources
            append j to destinations

write sources,destinations to CSV
return EdgeList
```

### Serial Notes

- Use `j = i + 1` to enforce the half-list directly.
- Compare squared distances to `cutoff^2` so we do not need a square root in the inner loop.
- Keep edge ordering deterministic by iterating `i` in ascending order and `j` in ascending order.
- Keep the output format simple: one CSV row per edge, with two columns, `source,destination`.
- The serial baseline does not need a spatial acceleration structure yet.
- If the `xyz` loader cannot parse some metadata, we can still inject the box and periodicity separately after loading.

## Frame API

For the baseline, `Frame` should expose only the information the neighbor search actually needs.

Suggested responsibilities:

- store coordinates
- store species IDs
- store box vectors or orthogonal box lengths
- store periodicity flags
- provide a squared-distance helper that already applies periodic boundary handling

Suggested interface:

```cpp
class Frame {
public:
    using coord_t = std::array<double, 3>;

    Frame() = default;
    size_t size() const noexcept;

    void load_from_extxyz(const std::string &path);

    double distance2(size_t i, size_t j) const;

    const std::vector<coord_t> &coords_ref() const noexcept;
    const std::vector<int> &species_ref() const noexcept;
    const std::array<std::array<double, 3>, 3> &box_ref() const noexcept;
    const std::array<bool, 3> &periodicity() const noexcept;

    void set_box(const std::array<std::array<double, 3>, 3> &box);
    void set_periodic(const std::array<bool, 3> &p);
};
```

The main reason to expose only `distance2(i, j)` is to keep the public API small. The cutoff test only needs a squared distance, and that is enough for the baseline algorithm.

## Output Format

The canonical serial output should be a two-column CSV file.

Conventions:

- use `0`-based atom indexing
- store each pair once
- prefer `i < j` ordering for symmetry
- write one edge per line as `source,destination`

This layout is simple to validate and easy to reuse later.

## Profiling Plan

At this stage, profiling should be designed as part of the implementation, not added later as an afterthought.

### What To Measure

- frame loading time
- neighbor-list build time
- output write time
- total wall-clock time for the full program

### How To Measure

- Use `std::chrono` timers inside the program for phase-level timings.
- Keep a small, always-on timing summary in the program output so every run is comparable.
- Use `/usr/bin/time -v` for a quick external check on wall time and memory when needed.
- Use a developed profiler as the main deep-dive tool:
  - `VTune` for serial and OpenMP CPU analysis
  - `Nsight Systems` or `Nsight Compute` later for CUDA analysis
- Treat profiler runs as selected experiments, not as the primary timing mechanism for every run.

### What To Profile First

- The brute-force pair loop will be the dominant cost.
- If timing is noisy, increase the problem size rather than profiling tiny inputs.
- Use the tiny validation frame only for correctness, not for performance conclusions.

### Evaluation Plan

- validation case: a tiny hand-built system with known expected edges
- benchmark case: a larger fixed-seed system used for timing and profiler runs
- same cutoff and box settings across repeated runs when comparing timing
- keep benchmark inputs stable so scaling plots, roofline analysis, and profiler screenshots all refer to the same representative workload

The best balance is:

- use built-in timers for nearly all benchmarking
- use VTune or Nsight on a smaller number of representative benchmark runs
- keep profiler output separate from correctness fixtures so the repo stays organized

## Validation vs Benchmarking

These are two different kinds of inputs, and they should be named differently.

- `validation` means small deterministic cases for correctness and debugging
- `benchmark` means larger fixed cases used for performance analysis and profiler runs

I would reserve the word `test` for validation-oriented cases, and use `benchmark` for profiling-oriented cases.

## Validation Strategy

We should keep at least one validation fixture:

1. A tiny deterministic frame with known ground-truth edges.

The validation frame should be small enough to inspect manually but not so small that it misses corner cases. A good target is roughly 6 to 12 atoms with a few obvious neighbor pairs and a few obvious non-neighbors.

Store this in `validation/` so it is shared by every implementation.

## Benchmark Strategy

We should keep at least one benchmark fixture:

1. A larger synthetic frame for timing experiments and profiling.

The benchmark case can be random, but it should be generated from a fixed seed so results are reproducible. This is the workload we will use for scaling curves and profiler sessions.

Store benchmark generators and generated benchmark frames in `benchmark/` so they can be reused across serial and parallel versions.

## OpenMP Plan

The OpenMP version should preserve the exact same neighbor-list semantics as the serial version:

- same cutoff rule
- same minimum-image rule
- same half-list convention `i < j`
- same final two-column edge output

The main goal is to parallelize the expensive pair-search phase while keeping correctness and a fair comparison against the serial baseline.

### What To Parallelize First

The first OpenMP implementation should parallelize the **edge-construction phase**, not the file loading phase.

Reasoning:

- the brute-force pair loop is the dominant `O(N^2)` cost
- file loading is `O(N)` and is unlikely to dominate runtime on benchmark-sized inputs
- parallel text parsing is possible, but it adds complexity early and is not the main HPC bottleneck
- keeping input loading serial in the first OpenMP version makes correctness and debugging much easier

So the recommended first OpenMP scope is:

1. load the extxyz frame serially
2. build the edge list in parallel
3. write the output serially

If profiling later shows that loading is a meaningful bottleneck, we can revisit parallel parsing as a secondary optimization.

### OpenMP Edge-Build Strategy

The outer loop over source atoms `i` is the natural loop to parallelize.

At a high level:

1. Load the frame.
2. Create a small per-source or per-thread edge container.
3. Parallelize the loop over `i`.
4. For each `i`, loop over `j > i`.
5. Compute the minimum-image squared distance.
6. If the pair is within cutoff, store `(i, j)` in the local container.
7. After the parallel region, stitch the local containers together into the final edge list.

### OpenMP Pseudocode

```text
load Frame
cutoff2 = cutoff * cutoff
create local edge containers

parallel for i in 0 .. N-1 schedule(dynamic)
    local container for this i or this thread
    for j in i+1 .. N-1:
        compute minimum-image squared distance
        if dist2 <= cutoff2:
            store (i, j) locally

merge local containers into final EdgeList
write final EdgeList
```

### Ordering Question

Yes, we should keep the final edge list in a deterministic order if we can do so without much extra complexity.

The cleanest approach is exactly the one you suggested:

- give each source atom `i` its own mini-container of destination indices
- let the thread responsible for `i` fill only that container
- after the parallel loop, concatenate the mini-containers in ascending `i` order

This has several advantages:

- no `critical` section is needed in the hot loop
- no atom pair is written by more than one thread
- the final edge order stays consistent with the serial half-list order
- validation against the serial output stays simple

This is probably the best first OpenMP design.

### Why Not Just Shared `push_back`?

A shared global edge list with `push_back` inside an OpenMP `critical` section is correct but not a good final design.

Why:

- every accepted edge contends on the same lock
- the hottest part of the computation becomes serialized
- scalability will be poor
- the resulting edge order may depend on thread timing

So `critical` is acceptable as a temporary prototype, but not as the implementation we want to benchmark seriously.

### Per-Atom Containers vs Per-Thread Containers

There are two reasonable designs:

1. Per-atom containers
2. Per-thread containers

For this project, I recommend **per-atom containers first**.

Why:

- preserves deterministic output order naturally
- easy to reason about
- easy to validate
- avoids merge ambiguity

The tradeoff is that you create many small containers, one for each source atom. That is usually acceptable for a first OpenMP implementation.

Per-thread containers may be slightly lighter-weight in some settings, but they make deterministic merging less direct.

### Scheduling Choice

Dynamic scheduling is a good first choice for the outer loop.

Why:

- early values of `i` have longer inner loops than late values
- the amount of work per iteration shrinks with increasing `i`
- dynamic scheduling helps balance that uneven work

This is worth testing later against `static`, but `dynamic` is a sensible default to start from.

### OpenMP Loading Discussion

Your loading idea is reasonable in principle, but I would not do it first.

What parallel loading would require:

- read the file into memory
- identify line boundaries
- preallocate coordinate/species arrays
- assign different atom-line ranges to different threads
- parse text into already-assigned slots instead of using `push_back`

That can work, but it changes the loader quite a bit and introduces complexity in:

- parsing the extxyz header
- splitting work by line rather than by bytes
- keeping parsing robust

For a project like this, I think the right strategy is:

- keep extxyz loading serial in the first OpenMP version
- optimize and parallelize the neighbor-search loop first
- only revisit loading if VTune later shows that parsing is nontrivial

### Recommendation

So my recommendation is:

1. Do **not** parallelize frame loading in the first OpenMP version.
2. Parallelize the outer `i` loop of neighbor-list construction.
3. Use **dynamic scheduling** initially.
4. Store results in **per-source mini-containers**.
5. Concatenate those mini-containers in `i` order to form the final edge list.

That gives a clean, scalable, and validation-friendly OpenMP design without introducing locks into the hot path.
