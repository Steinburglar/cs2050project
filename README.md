# CS2050: High Performance Computing Project

This repository contains the coursework project for CS2050 (High Performance Computing).

Overview
- Approaches: serial, OpenMP, MPI, CUDA, and additional experiments

Project assignment (summary)
The final project requires applying high-performance computing techniques to an algorithm or computational task. Start with a serial version, then develop parallel versions using OpenMP, MPI, and CUDA. Analyze scaling and profile performance, and briefly explore one additional programming paradigm. Produce a blog-style report with reproducible Slurm scripts and results.

Selected task: molecular-dynamics neighbor/adjacency construction
The chosen task is from molecular dynamics: given a 3D point cloud, build a sparse adjacency/neighbor list. The simplest variant is a binary neighbor list that marks pairs as neighbors if their distance is below a cutoff (often stored as a half-list due to symmetry). A richer variant uses a Gaussian distance kernel (or similar) to assign weighted edges rather than 0/1 values, which requires storing distances/weights alongside neighbor indices.

Does this meet the requirements?
Yes. This task is a compact, computationally meaningful problem that allows clear serial baseline, OpenMP/MPI/CUDA parallelizations, profiling, and additional experimentation (e.g., different kernels or spatial data structures). It is appropriately scoped for the course's expectations.

How to proceed
- Implement a correct serial neighbor-list builder in `serial/src/` and reproducible Slurm runs in `serial/slurm/`.
- Profile and optimize, then implement OpenMP, MPI, and CUDA variants, storing outputs and plots in each module's `results/`.
- Document methods, experiments, and conclusions in `report/report.md`.

Repository structure

cs2050project/
├── README.md
├── context.md
├── report/
│   └── report.md
│   └── figures/
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

Contact
- Author: (your name)
