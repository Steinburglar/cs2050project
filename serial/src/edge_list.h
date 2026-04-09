#ifndef CS2050_EDGE_LIST_H
#define CS2050_EDGE_LIST_H

#include <vector>
#include <cstdint>

struct EdgeList {
    using idx_t = int64_t;
    std::vector<idx_t> sources;
    std::vector<idx_t> destinations;
    std::vector<double> weights;

    void reserve(std::size_t n) {
        sources.reserve(n);
        destinations.reserve(n);
        weights.reserve(n);
    }
    void clear() {
        sources.clear();
        destinations.clear();
        weights.clear();
    }
    std::size_t size() const noexcept { return sources.size(); }
};

#endif // CS2050_EDGE_LIST_H
