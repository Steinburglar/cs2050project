# CS2050: High Performance Computing Project

This repository contains the coursework project for CS2050 (High Performance Computing).

Overview
- Approaches: serial, OpenMP, MPI, CUDA, and additional experiments

Project assignment (summary)
The final project requires applying high-performance computing techniques to an algorithm or computational task. Start with a serial version, then develop parallel versions using OpenMP, MPI, and CUDA. Analyze scaling and profile performance, and briefly explore one additional programming paradigm. Produce a blog-style report with reproducible Slurm scripts and results.

Selected task: molecular-dynamics neighbor-list construction
The chosen task is from molecular dynamics: given an `extxyz` point cloud, build a sparse binary neighbor list. The baseline output is a half-list that stores each atom pair once if its distance is below a cutoff. This project intentionally keeps the neighbor list binary to preserve a simple, stable data flow across the serial, OpenMP, MPI, and CUDA implementations.

Current comparison framing:
- serial, OpenMP, and CUDA are planned around the same global brute-force task for the cleanest baseline comparison
- MPI necessarily restructures the computation through spatial domain decomposition and halo exchange
- cell lists are being treated as possible later optimizations rather than part of the current baseline comparison

Does this meet the requirements?
Yes. This task is a compact, computationally meaningful problem that allows a clear serial baseline, OpenMP/MPI/CUDA parallelizations, profiling, and additional experimentation such as force computation or spatial acceleration structures. It is appropriately scoped for the course's expectations.

How to proceed
- Implement a correct serial neighbor-list builder in `serial/src/` and reproducible Slurm runs in `serial/slurm/`.
- Profile and optimize, then implement OpenMP, MPI, and CUDA variants, storing outputs and plots in each module's `results/`.
- Document methods, experiments, and conclusions in `report/report.md`.
- Use `validation/` for small correctness fixtures and `benchmark/` for reproducible performance inputs.
- See `PROJECT_SCOPE.md` for the current problem statement, API sketch, and data layout.
- See `algorithms.md` for the mid-level algorithm plan for each paradigm.

Repository structure
cs2050project/
├── README.md
├── context.md
├── report/
│   └── report.md
│   └── figures/
├── benchmark/
│   └── generate_benchmark_frame.py
├── validation/
│   └── validation_frame.xyz
│   └── validation_expected_edges.csv
├── serial/
│   └── src/
│   └── slurm/
│   └── results/
├── openmp/
│   └── src/
│   └── slurm/
│   └── results/
├── mpi/
│   └── src/
│   └── slurm/
│   └── results/
├── cuda/
│   └── src/
│   └── slurm/
│   └── results/
├── additional/
│   └── src/
│   └── slurm/
│   └── results/

How to use
- Add source code in the appropriate `*/src/` folder.
- Place Slurm job scripts in `*/slurm/` to reproduce runs.
- Save output, plots, and data in `*/results/`.
- The serial, OpenMP, and MPI executables all take an `extxyz` input and read box/periodicity directly from the file metadata. See [serial/README.md](/home/stein/classes/CS2050/cs2050project/serial/README.md) for the baseline CLI details.

Contact
- Author: (your name)
