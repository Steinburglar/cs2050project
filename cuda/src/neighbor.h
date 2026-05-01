#ifndef CS2050_CUDA_NEIGHBOR_H
#define CS2050_CUDA_NEIGHBOR_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <cuda_runtime.h>

#include "edge_list.h"

struct DeviceFrameBuffers
{
    int atom_count = 0;
    double *x = nullptr;
    double *y = nullptr;
    double *z = nullptr;
    double *box_diag = nullptr;
    int *periodic = nullptr;

    void release()
    {
        cudaFree(x);
        cudaFree(y);
        cudaFree(z);
        cudaFree(box_diag);
        cudaFree(periodic);
        x = nullptr;
        y = nullptr;
        z = nullptr;
        box_diag = nullptr;
        periodic = nullptr;
        atom_count = 0;
    }
};

struct DeviceEdgeBuffers
{
    EdgeList::idx_t *counts = nullptr;
    EdgeList::idx_t *offsets = nullptr;
    EdgeList::idx_t *sources = nullptr;
    EdgeList::idx_t *destinations = nullptr;
    std::size_t edge_count = 0;

    void release()
    {
        cudaFree(counts);
        cudaFree(offsets);
        cudaFree(sources);
        cudaFree(destinations);
        counts = nullptr;
        offsets = nullptr;
        sources = nullptr;
        destinations = nullptr;
        edge_count = 0;
    }
};

inline void check_cuda(cudaError_t err, const char *what)
{
    if (err != cudaSuccess)
    {
        throw std::runtime_error(std::string(what) + ": " + cudaGetErrorString(err));
    }
}

void build_edges_cuda_two_pass(const DeviceFrameBuffers &frame,
                               double cutoff,
                               DeviceEdgeBuffers &edges);

void download_edge_list_from_device(const DeviceEdgeBuffers &device_edges,
                                    EdgeList &host_edges);

#endif // CS2050_CUDA_NEIGHBOR_H
