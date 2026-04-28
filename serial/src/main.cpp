#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

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

/** Parse a boolean command-line flag such as 0/1 or true/false. */
static bool parse_bool_flag(const std::string &s)
{
    if (s == "1" || s == "true" || s == "True" || s == "T" || s == "t")
        return true;
    if (s == "0" || s == "false" || s == "False" || s == "F" || s == "f")
        return false;
    throw std::invalid_argument("invalid boolean flag: " + s);
}

/** Compare two orthogonal box descriptions using a small absolute tolerance. */
static bool boxes_match(const std::array<std::array<double, 3>, 3> &a,
                        const std::array<std::array<double, 3>, 3> &b,
                        double tol = 1e-12)
{
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            if (std::abs(a[i][j] - b[i][j]) > tol)
                return false;
        }
    }
    return true;
}

/** Compare two periodicity triplets exactly. */
static bool periodic_match(const std::array<bool, 3> &a, const std::array<bool, 3> &b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

/**
 * Build a half neighbor list with the current serial brute-force strategy.
 *
 * The loop only visits pairs with j > i so each edge is stored exactly once.
 * This is the simplest correct baseline for later parallel versions.
 */
static void build_edges_serial(const Frame &frame, double cutoff, EdgeList &out)
{
    out.clear();
    const double cutoff2 = cutoff * cutoff;
    const std::size_t N = frame.size();
    const auto &coords = frame.coords_ref();
    const auto &box = frame.box_ref();
    const auto &periodic = frame.periodicity();
    for (std::size_t i = 0; i < N; ++i)
    {
        for (std::size_t j = i + 1; j < N; ++j)
        {
            // Inline the minimum-image distance arithmetic here so the serial
            // baseline avoids an extra function call and bounds check for each pair.
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
                out.sources.push_back(static_cast<EdgeList::idx_t>(i));
                out.destinations.push_back(static_cast<EdgeList::idx_t>(j));
            }
        }
    }
}

/**
 * Serial entry point.
 *
 * Loads a single-frame extxyz file, verifies the provided box metadata,
 * runs the neighbor-list build, writes the half-list to CSV, and prints a
 * short summary of the result.
 */
int main(int argc, char **argv)
{
    if (argc != 10 && argc != 11)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <input.extxyz> <output.csv> <Lx> <Ly> <Lz> <px> <py> <pz> <cutoff> [--timing]\n";
        return 1;
    }

    const std::string input_path = argv[1];
    const std::string output_path = argv[2];
    std::array<std::array<double, 3>, 3> box{};
    box[0][0] = std::stod(argv[3]);
    box[1][1] = std::stod(argv[4]);
    box[2][2] = std::stod(argv[5]);
    const std::array<bool, 3> periodic{
        parse_bool_flag(argv[6]),
        parse_bool_flag(argv[7]),
        parse_bool_flag(argv[8])};
    const double cutoff = std::stod(argv[9]);
    const bool print_timing = (argc == 11 && std::string(argv[10]) == "--timing");

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

    if (!boxes_match(frame.box_ref(), box))
    {
        std::cerr << "Input box metadata does not match the box provided on the command line.\n";
        return 1;
    }
    if (!periodic_match(frame.periodicity(), periodic))
    {
        std::cerr << "Input periodicity metadata does not match the periodicity provided on the command line.\n";
        return 1;
    }

    frame.set_box(box);
    frame.set_periodic(periodic);

    std::cout << "Loaded frame with " << frame.size() << " atoms\n";
    std::cout << "Input: " << input_path << '\n';
    std::cout << "Output: " << output_path << '\n';
    std::cout << "Box: " << box[0][0] << ' ' << box[1][1] << ' ' << box[2][2] << '\n';
    std::cout << "Periodic: " << periodic[0] << ' ' << periodic[1] << ' ' << periodic[2] << '\n';

    EdgeList edges;
    {
        ScopedTimer timer(timing.build_ms);
        build_edges_serial(frame, cutoff, edges);
    }

    std::cout << "Found " << edges.size() << " edges (cutoff=" << cutoff << ")\n";

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

    const std::size_t show = std::min(edges.size(), static_cast<std::size_t>(10));
    std::cout << "First " << show << " edges:\n";
    for (std::size_t k = 0; k < show; ++k)
    {
        std::cout << std::setw(4) << edges.sources[k] << " -> " << std::setw(4) << edges.destinations[k] << "\n";
    }

    timing.total_ms = timing.load_ms + timing.build_ms + timing.write_ms;
    if (print_timing)
    {
        timing.print(std::cout);
    }

    return 0;
}
