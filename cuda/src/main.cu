#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "edge_list.h"
#include "frame.h"
#include "neighbor.h"
#include "timing.h"

struct HostFrameSoA
{
    std::vector<double> x;
    std::vector<double> y;
    std::vector<double> z;
    std::array<double, 3> box_diag{};
    std::array<int, 3> periodic{};
};

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

/** Repack the host Frame coordinates from AoS into SoA for CUDA upload. */
static HostFrameSoA pack_frame_soa(const Frame &frame)
{
    HostFrameSoA packed;
    const auto &coords = frame.coords_ref();
    packed.x.reserve(coords.size());
    packed.y.reserve(coords.size());
    packed.z.reserve(coords.size());

    for (const auto &coord : coords)
    {
        packed.x.push_back(coord[0]);
        packed.y.push_back(coord[1]);
        packed.z.push_back(coord[2]);
    }

    const auto &box = frame.box_ref();
    packed.box_diag = {box[0][0], box[1][1], box[2][2]};

    const auto &periodic = frame.periodicity();
    packed.periodic = {
        periodic[0] ? 1 : 0,
        periodic[1] ? 1 : 0,
        periodic[2] ? 1 : 0,
    };

    return packed;
}

/**
 * Allocate device buffers and copy a packed host frame into them.
 *
 * This is the host-side staging step immediately before the CUDA neighbor-list
 * kernel would run.
 * retuns a host- side pointer field to filled device buffers */
static DeviceFrameBuffers upload_frame_to_device(const HostFrameSoA &host)
{
    DeviceFrameBuffers device;
    device.atom_count = static_cast<int>(host.x.size());

    check_cuda(cudaMalloc(reinterpret_cast<void **>(&device.x), host.x.size() * sizeof(double)), "cudaMalloc(x)");
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&device.y), host.y.size() * sizeof(double)), "cudaMalloc(y)");
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&device.z), host.z.size() * sizeof(double)), "cudaMalloc(z)");
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&device.box_diag), 3 * sizeof(double)), "cudaMalloc(box_diag)");
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&device.periodic), 3 * sizeof(int)), "cudaMalloc(periodic)");

    check_cuda(cudaMemcpy(device.x, host.x.data(), host.x.size() * sizeof(double), cudaMemcpyHostToDevice), "cudaMemcpy(x)");
    check_cuda(cudaMemcpy(device.y, host.y.data(), host.y.size() * sizeof(double), cudaMemcpyHostToDevice), "cudaMemcpy(y)");
    check_cuda(cudaMemcpy(device.z, host.z.data(), host.z.size() * sizeof(double), cudaMemcpyHostToDevice), "cudaMemcpy(z)");
    check_cuda(cudaMemcpy(device.box_diag, host.box_diag.data(), 3 * sizeof(double), cudaMemcpyHostToDevice), "cudaMemcpy(box_diag)");
    check_cuda(cudaMemcpy(device.periodic, host.periodic.data(), 3 * sizeof(int), cudaMemcpyHostToDevice), "cudaMemcpy(periodic)");

    return device;
}

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
    {
        ScopedTimer timer(timing.load_ms);
        frame.load_from_extxyz(input_path, false);
    }

    HostFrameSoA packed;
    DeviceFrameBuffers device;
    DeviceEdgeBuffers device_edges;
    EdgeList host_edges;
    {
        ScopedTimer timer(timing.pack_ms);
        packed = pack_frame_soa(frame);
    }
    {
        ScopedTimer timer(timing.upload_ms);
        device = upload_frame_to_device(packed);
    }
    {
        ScopedTimer timer(timing.build_ms);
        build_edges_cuda_two_pass(device, cutoff, device_edges);
    }

    std::cout << "Loaded frame with " << frame.size() << " atoms\n";
    std::cout << "Input: " << input_path << '\n';
    std::cout << "Output: " << (skip_write ? "<disabled>" : output_path) << '\n';
    std::cout << "Cutoff: " << cutoff << '\n';
    std::cout << "Packed host coordinates into SoA buffers and uploaded them to device memory\n";
    std::cout << "Device atom buffers: " << device.atom_count << " atoms across x/y/z arrays\n";
    std::cout << "Found " << device_edges.edge_count << " edges on device\n";

    if (!skip_write)
    {
        ScopedTimer timer(timing.write_ms);
        download_edge_list_from_device(device_edges, host_edges);
        write_edges_csv(host_edges, output_path);
    }

    device_edges.release();
    device.release();

    timing.total_ms = timing.load_ms + timing.pack_ms + timing.upload_ms + timing.build_ms + timing.write_ms;
    if (print_timing)
    {
        timing.print(std::cout);
    }

    return 0;
}
