#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <omp.h>

#include "frame.h"
#include "edge_list.h"
#include "timing.h"

/** Write the edge list as a two-column CSV file: source,destination. */
static void write_edges_csv(const EdgeList &edges, const std::string &path)
{
    std::ofstream out(path);
    if (!out)
    {
        throw std::runtime_error("failed to open output file " + path);
    }

    for (std::size_t k = 0; k < edges.size(); ++k)
    {
        out << edges.sources[k] << ',' << edges.destinations[k] << '\n';
    }
}

/**
 * Build a half neighbor list with an OpenMP-parallel outer loop.
 *
 * Each source atom owns its own local destination container, which preserves
 * deterministic final ordering and avoids synchronization in the hot loop.
 */
static void build_edges_openmp(const Frame &frame, double cutoff, EdgeList &out)
{
    out.clear();
    const double cutoff2 = cutoff * cutoff;
    const std::size_t N = frame.size();
    const auto &coords = frame.coords_ref();
    const auto &box = frame.box_ref();
    const auto &periodic = frame.periodicity();

    std::vector<std::vector<EdgeList::idx_t>> per_source_destinations(N);

#pragma omp parallel for schedule(dynamic)
    for (std::size_t i = 0; i < N; ++i)
    {
        auto &destinations = per_source_destinations[i];
        destinations.reserve((N > i + 1) ? std::min<std::size_t>(64, N - i - 1) : 0);

        for (std::size_t j = i + 1; j < N; ++j)
        {
            double dx = coords[j][0] - coords[i][0];
            double dy = coords[j][1] - coords[i][1];
            double dz = coords[j][2] - coords[i][2];

            if (periodic[0])
            {
                const double Lx = box[0][0];
                if (Lx > 0.0)
                {
                    if (dx > 0.5 * Lx)
                        dx -= Lx;
                    else if (dx < -0.5 * Lx)
                        dx += Lx;
                }
            }
            if (periodic[1])
            {
                const double Ly = box[1][1];
                if (Ly > 0.0)
                {
                    if (dy > 0.5 * Ly)
                        dy -= Ly;
                    else if (dy < -0.5 * Ly)
                        dy += Ly;
                }
            }
            if (periodic[2])
            {
                const double Lz = box[2][2];
                if (Lz > 0.0)
                {
                    if (dz > 0.5 * Lz)
                        dz -= Lz;
                    else if (dz < -0.5 * Lz)
                        dz += Lz;
                }
            }

            const double dist2 = dx * dx + dy * dy + dz * dz;
            if (dist2 <= cutoff2)
            {
                destinations.push_back(static_cast<EdgeList::idx_t>(j));
            }
        }
    }

    std::size_t total_edges = 0;
    for (const auto &destinations : per_source_destinations)
    {
        total_edges += destinations.size();
    }
    out.reserve(total_edges);

    for (std::size_t i = 0; i < N; ++i)
    {
        const auto &destinations = per_source_destinations[i];
        for (const auto j : destinations)
        {
            out.sources.push_back(static_cast<EdgeList::idx_t>(i));
            out.destinations.push_back(j);
        }
    }
}

/**
 * OpenMP entry point.
 *
 * Loads a single-frame extxyz file, runs the parallel neighbor-list build,
 * writes the half-list to CSV, and prints a short summary of the result.
 */
int main(int argc, char **argv)
{
    if (argc < 4 || argc > 6)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <input.extxyz> <output.csv|-> <cutoff> [--timing] [--no-write]\n";
        return 1;
    }

    const std::string input_path = argv[1];
    const std::string output_path = argv[2];
    const double cutoff = std::stod(argv[3]);
    bool print_timing = false;
    bool skip_write = (output_path == "-");
    for (int argi = 4; argi < argc; ++argi)
    {
        const std::string flag = argv[argi];
        if (flag == "--timing")
        {
            print_timing = true;
        }
        else if (flag == "--no-write")
        {
            skip_write = true;
        }
        else
        {
            std::cerr << "Unknown flag: " << flag << '\n';
            return 1;
        }
    }

    TimingSummary timing;
    Frame frame;
    try
    {
        ScopedTimer timer(timing.load_ms);
        frame.load_from_extxyz(input_path, false);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to load frame: " << e.what() << '\n';
        return 1;
    }

    std::cout << "Loaded frame with " << frame.size() << " atoms\n";
    std::cout << "Input: " << input_path << '\n';
    std::cout << "Output: " << (skip_write ? "<disabled>" : output_path) << '\n';
    const auto &box = frame.box_ref();
    const auto &periodic = frame.periodicity();
    std::cout << "Box: " << box[0][0] << ' ' << box[1][1] << ' ' << box[2][2] << '\n';
    std::cout << "Periodic: " << periodic[0] << ' ' << periodic[1] << ' ' << periodic[2] << '\n';
    std::cout << "OpenMP threads: " << omp_get_max_threads() << '\n';

    EdgeList edges;
    {
        ScopedTimer timer(timing.build_ms);
        build_edges_openmp(frame, cutoff, edges);
    }

    std::cout << "Found " << edges.size() << " edges (cutoff=" << cutoff << ")\n";

    if (!skip_write)
    {
        try
        {
            ScopedTimer timer(timing.write_ms);
            write_edges_csv(edges, output_path);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to write edge list: " << e.what() << '\n';
            return 1;
        }
    }

    if (!skip_write)
    {
        const std::size_t show = std::min(edges.size(), static_cast<std::size_t>(10));
        std::cout << "First " << show << " edges:\n";
        for (std::size_t k = 0; k < show; ++k)
        {
            std::cout << std::setw(4) << edges.sources[k] << " -> " << std::setw(4) << edges.destinations[k] << "\n";
        }
    }

    timing.total_ms = timing.load_ms + timing.build_ms + timing.write_ms;
    if (print_timing)
    {
        timing.print(std::cout);
    }

    return 0;
}
