# CS2050 Final Project Scope

## Core Goal

Build and compare high-performance implementations of a molecular-dynamics neighbor-list constructor.

The core problem is:

- Input: a 3D point cloud of atoms, plus periodic box information and a cutoff radius.
- Output: a **half neighbor list** containing each atom pair exactly once when the pair distance is less than or equal to the cutoff.

The project will start with a serial C++ implementation and then extend the same algorithm to OpenMP, MPI, CUDA, and one additional paradigm.

## What The Neighbor List Means

For atoms `i` and `j`:

- compute the distance using the minimum-image convention when periodic boundaries are enabled
- include the pair if `distance(i, j) <= cutoff`
- store the pair once, not twice, using a half-list convention such as `i < j`

The neighbor list is binary:

- a pair is either present or absent
- no weights column is included
- no force computation is required for the baseline

## Baseline Scope

The first milestone is a correct, simple, reproducible serial program that:

- loads an `xyz` frame of atom positions
- applies box and periodic-boundary settings
- constructs the half neighbor list
- writes the resulting edges to a CSV file
- measures runtime for performance comparison

The small correctness fixture should live in `validation/`, and benchmark frame generation should live in `benchmark/`.

This baseline is intentionally simple. The goal is to keep the data flow stable so every parallel backend can reuse the same semantics.

## Stretch Goal

If time allows, the main extension should be:

- compute pairwise forces for one timestep using the neighbor list

This is a better extension than adding weights to the neighbor list because it:

- stays close to molecular-dynamics use cases
- introduces reduction and race-condition issues that are useful for HPC analysis
- builds naturally on top of the same neighbor-list infrastructure

## Core API

The project should keep a small, stable API that all implementations can share.

### `Frame`

`Frame` owns the atom data and simulation metadata.

Suggested responsibilities:

- store coordinates
- store species IDs
- store box vectors or orthogonal box lengths
- store periodicity flags
- provide distance and minimum-image helpers

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

### `EdgeList`

`EdgeList` is the canonical output container for the half neighbor list.

Suggested responsibilities:

- store edge endpoints in structure-of-arrays form
- provide simple reserve, clear, and size helpers

Suggested interface:

```cpp
struct EdgeList {
    using idx_t = int64_t;

    std::vector<idx_t> sources;
    std::vector<idx_t> destinations;

    void reserve(std::size_t n);
    void clear();
    std::size_t size() const noexcept;
};
```

## Data Layout

The canonical neighbor-list output should be a flat edge list in structure-of-arrays form:

- `sources[k]` is the source atom index for edge `k`
- `destinations[k]` is the destination atom index for edge `k`
- edge `k` represents one half-list pair

Conventions:

- use `0`-based atom indexing
- store each pair once
- prefer `i < j` ordering for symmetry
- use `int64_t` for indices unless a smaller type is clearly sufficient

This layout is simple, easy to validate, and easy to share across serial, OpenMP, MPI, and CUDA implementations.

## Implementation Strategy

The project should follow a top-down progression:

1. Serial brute-force baseline
2. OpenMP shared-memory version
3. MPI distributed-memory version
4. CUDA GPU version
5. Additional paradigm or framework

The first version can be brute-force `O(N^2)` pair checking. If needed later, we can introduce a spatial acceleration structure such as cell lists, but that should only happen if it helps performance or clarity without forcing a redesign.

## Parallelization Notes

- OpenMP: parallelize the outer loop, but avoid shared `push_back` contention by using per-thread buffers or a two-pass build.
- MPI: domain decomposition plus halo exchange is the natural path.
- CUDA: use a GPU-friendly counting and fill strategy rather than dynamic allocation inside kernels.

## What Is Out Of Scope For The Baseline

The baseline project does not need:

- weighted edges
- a dense adjacency matrix
- full MD integration
- energy minimization
- complex chemistry-specific data structures

Those can be added later only if they clearly support the performance study.
