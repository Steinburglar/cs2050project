/**
 * @file edge_list.h
 * @brief Compact structure-of-arrays storage for a half neighbor list.
 */
#ifndef CS2050_EDGE_LIST_H
#define CS2050_EDGE_LIST_H

#include <cstdint>
#include <vector>

/** Simple flat edge container used by the serial and parallel builders. */
struct EdgeList
{
    /** Integer type used for atom indices. */
    using idx_t = int64_t;

    /** Source atom index for each stored edge. */
    std::vector<idx_t> sources;
    /** Destination atom index for each stored edge. */
    std::vector<idx_t> destinations;

    /** Reserve space for roughly @p n edges in each array. */
    void reserve(std::size_t n)
    {
        sources.reserve(n);
        destinations.reserve(n);
    }
    /** Remove all stored edges without releasing capacity. */
    void clear()
    {
        sources.clear();
        destinations.clear();
    }
    /** Return the number of stored edges. */
    std::size_t size() const noexcept { return sources.size(); }
};

#endif // CS2050_EDGE_LIST_H
