#include <iostream>
#include <iomanip>
#include <algorithm>
#include <array>
#include <omp.h>

#include "frame.h"
#include "edge_list.h"

// OpenMP parallel serial builder.
// Note: using `omp critical` to protect push_back calls here. A fine-grained atomic on push_back
// isn't possible; atomics on scalars are possible but push_back performs potential reallocation
// and is not atomic. For good performance, prefer two-phase counting + fill or per-thread buffers.
static void build_edges_openmp(const Frame &frame, double cutoff, EdgeList &out) {
    out.clear();
    const std::size_t N = frame.size();

    // Parallelize outer loop. Use dynamic scheduling to balance work across uneven neighborhoods.
#pragma omp parallel for schedule(dynamic)
    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = i + 1; j < N; ++j) {
            double d = frame.distance(i, j);
            if (d <= cutoff) {
                // protect vector updates with a critical section. This is simple but may limit scalability.
#pragma omp critical
                {
                    out.sources.push_back(static_cast<EdgeList::idx_t>(i));
                    out.destinations.push_back(static_cast<EdgeList::idx_t>(j));
                    out.weights.push_back(1.0);
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    const char *path = "../serial/tests/sample_frame.csv";
    if (argc > 1) path = argv[1];

    Frame frame;
    try {
        frame.load_from_simple_file(path);
    } catch (const std::exception &e) {
        std::cerr << "Failed to load frame: " << e.what() << '\n';
        return 1;
    }

    std::array<std::array<double,3>,3> box{};
    box[0][0] = 50.0; box[1][1] = 50.0; box[2][2] = 50.0;
    frame.set_box(box);
    frame.set_periodic({true,true,true});

    std::cout << "Loaded frame with " << frame.size() << " atoms\n";

    EdgeList edges;
    double cutoff = 5.0;
    build_edges_openmp(frame, cutoff, edges);

    std::cout << "Found " << edges.size() << " edges (cutoff=" << cutoff << ")\n";
    const std::size_t show = std::min(edges.size(), static_cast<std::size_t>(10));
    std::cout << "First " << show << " edges:\n";
    for (std::size_t k = 0; k < show; ++k) {
        std::cout << std::setw(4) << edges.sources[k] << " -> " << std::setw(4) << edges.destinations[k]
                  << "  w=" << edges.weights[k] << "\n";
    }

    return 0;
}
