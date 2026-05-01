#include "neighbor.h"

#include <thrust/device_ptr.h>
#include <thrust/scan.h>

/**
 * Compute squared minimum-image distance between two atoms already resident in
 * device memory. This mirrors the CPU-side orthogonal-box logic.
 */
__device__ double minimum_image_distance2_device(const double *x,
                                                 const double *y,
                                                 const double *z,
                                                 const double *box_diag,
                                                 const int *periodic,
                                                 int i,
                                                 int j)
{
    double dx = x[j] - x[i];
    double dy = y[j] - y[i];
    double dz = z[j] - z[i];

    if (periodic[0] != 0)
    {
        const double Lx = box_diag[0];
        if (Lx > 0.0)
        {
            if (dx > 0.5 * Lx)
                dx -= Lx;
            else if (dx < -0.5 * Lx)
                dx += Lx;
        }
    }
    if (periodic[1] != 0)
    {
        const double Ly = box_diag[1];
        if (Ly > 0.0)
        {
            if (dy > 0.5 * Ly)
                dy -= Ly;
            else if (dy < -0.5 * Ly)
                dy += Ly;
        }
    }
    if (periodic[2] != 0)
    {
        const double Lz = box_diag[2];
        if (Lz > 0.0)
        {
            if (dz > 0.5 * Lz)
                dz -= Lz;
            else if (dz < -0.5 * Lz)
                dz += Lz;
        }
    }

    return dx * dx + dy * dy + dz * dz;
}

/**
 * Pass 1: one thread owns one source atom i and counts how many neighbors it
 * has among all j > i. This deliberately mirrors the global brute-force CPU
 * baselines for fairness.
 */
__global__ void count_neighbors_kernel(const double *x,
                                       const double *y,
                                       const double *z,
                                       const double *box_diag,
                                       const int *periodic,
                                       int atom_count,
                                       double cutoff2,
                                       EdgeList::idx_t *counts)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= atom_count)
    {
        return;
    }

    EdgeList::idx_t neighbor_count = 0;
    for (int j = i + 1; j < atom_count; ++j)
    {
        if (minimum_image_distance2_device(x, y, z, box_diag, periodic, i, j) <= cutoff2)
        {
            neighbor_count += 1;
        }
    }

    counts[i] = neighbor_count;
}

/**
 * Pass 2: the same thread/source-atom mapping is used again, but now each
 * thread already knows where its output segment begins through offsets[i].
 */
__global__ void fill_neighbors_kernel(const double *x,
                                      const double *y,
                                      const double *z,
                                      const double *box_diag,
                                      const int *periodic,
                                      int atom_count,
                                      double cutoff2,
                                      const EdgeList::idx_t *offsets, // list of the starting index in the output arrays for each source atom
                                      EdgeList::idx_t *sources,
                                      EdgeList::idx_t *destinations)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= atom_count)
    {
        return;
    }

    EdgeList::idx_t write_ptr = offsets[i];
    for (int j = i + 1; j < atom_count; ++j)
    {
        if (minimum_image_distance2_device(x, y, z, box_diag, periodic, i, j) <= cutoff2)
        {
            sources[write_ptr] = static_cast<EdgeList::idx_t>(i);
            destinations[write_ptr] = static_cast<EdgeList::idx_t>(j);
            write_ptr += 1;
        }
    }
}

void build_edges_cuda_two_pass(const DeviceFrameBuffers &frame,
                               double cutoff,
                               DeviceEdgeBuffers &edges)
{
    edges.release();

    if (frame.atom_count == 0)
    {
        return;
    }

    const std::size_t count_bytes = static_cast<std::size_t>(frame.atom_count) * sizeof(EdgeList::idx_t);
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&edges.counts), count_bytes), "cudaMalloc(counts)");
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&edges.offsets), count_bytes), "cudaMalloc(offsets)");

    const int threads_per_block = 256;
    const int blocks = (frame.atom_count + threads_per_block - 1) / threads_per_block;
    const double cutoff2 = cutoff * cutoff;

    count_neighbors_kernel<<<blocks, threads_per_block>>>(
        frame.x, frame.y, frame.z, frame.box_diag, frame.periodic,
        frame.atom_count, cutoff2, edges.counts);
    check_cuda(cudaGetLastError(), "count_neighbors_kernel launch");
    check_cuda(cudaDeviceSynchronize(), "count_neighbors_kernel sync");

    thrust::device_ptr<EdgeList::idx_t> counts_ptr(edges.counts);
    thrust::device_ptr<EdgeList::idx_t> offsets_ptr(edges.offsets);
    thrust::exclusive_scan(counts_ptr, counts_ptr + frame.atom_count, offsets_ptr);

    EdgeList::idx_t last_count = 0;
    EdgeList::idx_t last_offset = 0;
    check_cuda(cudaMemcpy(&last_count, edges.counts + (frame.atom_count - 1),
                          sizeof(EdgeList::idx_t), cudaMemcpyDeviceToHost),
               "cudaMemcpy(last_count)");
    check_cuda(cudaMemcpy(&last_offset, edges.offsets + (frame.atom_count - 1),
                          sizeof(EdgeList::idx_t), cudaMemcpyDeviceToHost),
               "cudaMemcpy(last_offset)");

    edges.edge_count = static_cast<std::size_t>(last_offset + last_count);
    if (edges.edge_count == 0)
    {
        return;
    }

    const std::size_t edge_bytes = edges.edge_count * sizeof(EdgeList::idx_t);
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&edges.sources), edge_bytes), "cudaMalloc(sources)");
    check_cuda(cudaMalloc(reinterpret_cast<void **>(&edges.destinations), edge_bytes), "cudaMalloc(destinations)");

    fill_neighbors_kernel<<<blocks, threads_per_block>>>(
        frame.x, frame.y, frame.z, frame.box_diag, frame.periodic,
        frame.atom_count, cutoff2, edges.offsets, edges.sources, edges.destinations);
    check_cuda(cudaGetLastError(), "fill_neighbors_kernel launch");
    check_cuda(cudaDeviceSynchronize(), "fill_neighbors_kernel sync");
}

void download_edge_list_from_device(const DeviceEdgeBuffers &device_edges,
                                    EdgeList &host_edges)
{
    host_edges.clear();
    host_edges.reserve(device_edges.edge_count);

    if (device_edges.edge_count == 0)
    {
        return;
    }

    host_edges.sources.resize(device_edges.edge_count);
    host_edges.destinations.resize(device_edges.edge_count);

    const std::size_t edge_bytes = device_edges.edge_count * sizeof(EdgeList::idx_t);
    check_cuda(cudaMemcpy(host_edges.sources.data(), device_edges.sources,
                          edge_bytes, cudaMemcpyDeviceToHost),
               "cudaMemcpy(sources->host)");
    check_cuda(cudaMemcpy(host_edges.destinations.data(), device_edges.destinations,
                          edge_bytes, cudaMemcpyDeviceToHost),
               "cudaMemcpy(destinations->host)");
}
