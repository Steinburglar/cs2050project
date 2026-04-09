Project context and agent notes

Status
- Repository initialized as a git repo.
- Initial scaffolding created: `README.md`, this `context.md`, `report/report.md`, and placeholder folders for serial/openmp/mpi/cuda/additional.

Goals
- Implement and compare multiple implementations of the assigned computational problem (serial, OpenMP, MPI, CUDA).
- Produce reproducible runs via Slurm scripts and collect results, timings, and plots.
- Write a final report summarizing methods, performance, and conclusions.

Project selection
Assignment summary: The final project requires a serial baseline, OpenMP/MPI/CUDA parallel versions, profiling and scaling analysis, and a blog-style report with reproducible Slurm scripts.

Chosen task: molecular-dynamics neighbor/adjacency construction
- Core task: Given a 3D point cloud, build a sparse neighbor list (pairs with distance <= cutoff). Use a half-list representation to exploit symmetry.
- Extension: Replace binary adjacency with a Gaussian distance kernel (store weights/distances), or explore spatial acceleration structures (cell lists, KD-trees).

Why this fits: The task is computationally meaningful, easy to test at multiple scales, and supports distinct parallel strategies (thread-level work distribution, domain decomposition for MPI, GPU kernels for CUDA).

Next implementation steps
1. Implement serial neighbor-list builder in `serial/src/` (baseline correctness and timings).
2. Create `serial/slurm/` scripts to run baselines at multiple problem sizes and store outputs in `serial/results/`.
3. Implement OpenMP (shared-memory) and profile hotspots.
4. Implement MPI (domain decomposition / distributed memory) and validate scaling.
5. Implement CUDA version(s) and compare GPU speedups.
6. Explore one additional paradigm (e.g., vectorized math libraries, Python+Numba, or a different kernel) and document findings.

Immediate next steps
1. Add problem description and requirements to `README.md` and `report/report.md`.
2. Add serial implementation prototypes in `serial/src/` and Slurm scripts in `serial/slurm/`.
3. Run baseline serial experiments and store outputs in `serial/results/`.
4. Incrementally implement OpenMP, MPI, and CUDA versions, reproducing experiments with Slurm scripts.

Notes for the agent
- Use this file to track repository state, decisions, and remaining tasks.
- Update statuses here after major changes (code added, experiments run, report updated).

Design notes — input formats and core data structures

Input format
- Primary input: an N x 4 array (or file) where each row is: `(x, y, z, species_id)`. `species_id` is an integer code for chemical species.
- Additional frame metadata: periodic box (3 vectors or box extents), periodicity flags (bool for each dimension), optional `frame_label` string.
- We will accept a simple CSV / whitespace text format for initial development. Later we can add parsers to read ASE `Atoms` objects via Python and pass them into C++ (see `pybind` notes below).

`Frame` class (C++ sketch)
- Purpose: hold coordinates, species array, box, periodicity, and simple utilities.
- Suggested fields:
	- `std::vector<std::array<double,3>> coords;`  // length N
	- `std::vector<int> species;`  // length N
	- `std::array<std::array<double,3>,3> box;`   // box vectors or identity for orthogonal boxes
	- `std::array<bool,3> periodic;`
	- `std::string label;`
- Suggested methods:
	- `size_t size() const;`
	- `void load_from_simple_file(const std::string &path);`  // CSV/whitespace loader for debugging
	- `std::array<double,3> minimum_image_disp(int i, int j) const;` // return displacement obeying PBC
	- `double distance(int i, int j) const;` // using minimum-image if periodic

Data outputs (semantic options)
1) Per-atom neighbor lists: `std::vector<std::vector<int>> neighbors;` where `neighbors[i]` lists j indices.
2) Half-neighbor lists: same as (1) but enforce `i>j` or `i<j` ordering when adding to remove redundancy.
3) Full adjacency matrix: dense `N x N` matrix (not memory efficient for large N) — use only for small tests.
4) Half/triangular adjacency: store only upper or lower triangle to save memory; representation depends on downstream consumers.
5) Weighted adjacency: store edges with weights `float/double` — e.g., `std::vector<std::vector<std::pair<int,double>>>` or a CSR-style sparse matrix for efficiency.

Representation choices and tradeoffs
- Neighbor-list (`vector<vector<int>>`): simple to construct and iterate; good for binary adjacency and half-lists. Memory proportional to number of edges.
- Weighted neighbor-list (`vector<vector<pair<int,double>>>`): natural extension for kernels that require storing weights.
- Sparse matrix (CSR / compressed sparse row): good for numerical linear-algebra style consumers and compact storage of weighted graphs. Slightly more complex to build but much more memory-efficient than dense matrices.
- Dense `N x N` array: simplest to reason about; only suitable for tiny N due to O(N^2) memory.

Parallelization and algorithm strategy (serial → optimized)
- Baseline: brute-force O(N^2) pairwise test with minimum-image distance and cutoff. Implement first for correctness and easy testing.
- Acceleration (next): cell lists / uniform grid (linear in N + neighborhood), Verlet lists for dynamic simulations, or spatial trees (k-d tree) for different use cases.
- Shared-memory parallelism (OpenMP): parallelize outer loop over atoms; be careful with building neighbor lists to avoid concurrent push_back contention (pre-allocate or use per-thread buffers and merge).
- Distributed-memory (MPI): domain-decompose by spatial partitions; exchange halo atoms across neighbors and construct local neighbor lists. For weighted adjacency store local CSR pieces and optionally assemble a global sparse matrix if needed.
- CUDA: use spatial hashing / cell lists to map neighbor-search to GPU kernels; store results in pre-allocated arrays or use two-phase counting + fill to avoid dynamic allocations on GPU.

Interoperability with Python / ASE (pybind outline)
- Approach: keep C++ `Frame` and neighbor search API, expose a thin wrapper with `pybind11`.
- Typical flow: in Python, create or load `ase.Atoms`; extract `positions` and `numbers` arrays and call the bound C++ constructor or factory function that accepts numpy arrays (`pybind11::array_t<double>` and `pybind11::array_t<int>`).
- `pybind11` handles conversion from numpy to contiguous buffers; C++ can copy or reference data depending on lifetime requirements. This avoids directly passing ASE `Atoms` objects — instead pass raw arrays extracted from ASE.
- We'll add a note and minimal example (Python) later when we implement bindings.

API and files to add
- `serial/src/frame.h` / `frame.cpp`: `Frame` class and simple file loader.
- `serial/src/neighbor_bruteforce.cpp`: serial O(N^2) neighbor finder with simple CLI driver.`
- `serial/src/main.cpp`: small driver that reads a sample frame and prints neighbor counts or timings.
- `serial/slurm/example_serial.sh`: example Slurm script showing how to build/run the serial executable.
- `serial/tests/sample_frame.txt` or `.csv`: small test frame to validate correctness.

Next steps (immediate)
1. Implement `Frame` class and `load_from_simple_file` (I can scaffold this if you want, or you implement and I review).
2. Implement brute-force neighbor finder in `serial/src/` and a tiny `main` to exercise it.
3. Add unit test `serial/tests/` with a tiny frame and expected neighbor list for validation.
4. After serial correctness, optimize with cell lists and add OpenMP parallelization.

Notes about extensibility
- Design APIs so neighbor-search functions take a cutoff and a `weight_function` callback (for weighted adjacency). That way the same loop can compute binary or weighted results with minimal changes.
- For outputs, provide writer utilities to emit neighbor-lists in plain text and optionally export CSR arrays for downstream tools.

Chosen output: Edge-list (Structure of Arrays)

Decision
- The canonical output format for this project will be an edge-list in Structure-of-Arrays (SoA) layout: three parallel arrays describing the graph edges.

Layout details
- `sources[]` (integer indices) — source node index for each edge.
- `destinations[]` (integer indices) — destination node index for each edge.
- `weights[]` (double/float) — weight associated with the edge (use `1.0` for binary neighbor lists).
- All three arrays have the same length `M` (the number of stored edges). Entry `k` represents an edge from `sources[k]` to `destinations[k]` with weight `weights[k]`.

Conventions
- Use 0-based indexing for atom indices (C++ native style).
- For symmetric relationships, we will prefer a half-list representation by default (store only pairs with `i<j`), unless a full directed list is explicitly requested.
- Types: use `int32_t` or `int64_t` for indices depending on expected `N` (use `int64_t` if `N` may exceed 2^31), and `double` for weights.

Building strategy (C++ implementation notes)
- Two-phase approach (recommended):
	1. Counting pass: iterate over pairs (or use cell lists) and count how many edges will be emitted per source (or globally). This lets us pre-allocate arrays of exact size `M`.
	2. Fill pass: iterate again and write into `sources[k]`, `destinations[k]`, `weights[k]` at computed offsets.
- Alternative simpler approach for development: push into `std::vector<int>` / `std::vector<double>` dynamically and move into contiguous arrays at the end. This is easier to implement but may allocate frequently for large datasets.
- For threaded/OpenMP builds, avoid concurrent push_back into shared vectors — prefer per-thread local buffers + concatenation, or use the two-phase counting + prefix-sum offsets so threads fill distinct ranges without contention.

Interfacing from Python / numpy
- Typical flow: start from an `N x 4` numpy array (positions + species) in Python. Extract coordinate and species arrays and pass them to C++ via `pybind11` as `pybind11::array_t<double>` and `pybind11::array_t<int>`.
- C++ code will construct a `Frame` from the numpy data (copy or reference depending on lifetime) and then run the neighbor/edge builder to produce the three arrays. `pybind11` can return numpy arrays created from the C++ buffers without copying if desired.

Writers and interoperability
- Provide simple writers to dump SoA edge-lists as CSV or binary files for downstream visualization and validation.
- Provide optional export to CSR (row pointer, col index, values) when consumers expect sparse-matrix formats.



