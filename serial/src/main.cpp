#include <iostream>
#include <iomanip>
#include <algorithm>
#include <array>

#include "frame.h"
#include "edge_list.h"

// Serial edge builder (half-list). Keeps implementation simple for debugging.
static void build_edges_serial(const Frame &frame, double cutoff, EdgeList &out)
{
    out.clear();
    const std::size_t N = frame.size();
    for (std::size_t i = 0; i < N; ++i)
    {
        for (std::size_t j = i + 1; j < N; ++j)
        {
            double d = frame.distance(i, j);
            if (d <= cutoff)
            {
                out.sources.push_back(static_cast<EdgeList::idx_t>(i));
                out.destinations.push_back(static_cast<EdgeList::idx_t>(j));
                out.weights.push_back(1.0);
            }
        }
    }
}

int main(int argc, char **argv)
{
    const char *path = "tests/sample_frame.csv";
    if (argc > 1)
        path = argv[1];

    Frame frame;
    try
    {
        frame.load_from_simple_file(path);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to load frame: " << e.what() << '\n';
        return 1;
    }

    // set periodic 50x50x50 box
    std::array<std::array<double, 3>, 3> box{};
    box[0][0] = 50.0;
    box[1][1] = 50.0;
    box[2][2] = 50.0;
    frame.set_box(box);
    frame.set_periodic({true, true, true});

    std::cout << "Loaded frame with " << frame.size() << " atoms\n";

    EdgeList edges;
    double cutoff = 5.0;
    build_edges_serial(frame, cutoff, edges);

    std::cout << "Found " << edges.size() << " edges (cutoff=" << cutoff << ")\n";
    const std::size_t show = std::min(edges.size(), static_cast<std::size_t>(10));
    std::cout << "First " << show << " edges:\n";
    for (std::size_t k = 0; k < show; ++k)
    {
        std::cout << std::setw(4) << edges.sources[k] << " -> " << std::setw(4) << edges.destinations[k]
                  << "  w=" << edges.weights[k] << "\n";
    }

    return 0;
}
