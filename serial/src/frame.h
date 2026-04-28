/**
 * @file frame.h
 * @brief Atom-frame container and distance helper for the serial neighbor-list builder.
 */
#ifndef CS2050_FRAME_H
#define CS2050_FRAME_H

#include <vector>
#include <array>
#include <string>
#include <cstdint>

class Frame
{
public:
    /** 3D coordinate type used throughout the project. */
    using coord_t = std::array<double, 3>;

    /** Construct an empty frame. */
    Frame() = default;

    /**
     * Construct a frame from already-populated data.
     *
     * @param coords Atom coordinates.
     * @param species Integer species labels, one per atom.
     * @param box Periodic box matrix. For now we assume an orthogonal box.
     * @param periodic Per-dimension periodicity flags.
     * @param label Optional frame label.
     */
    Frame(std::vector<coord_t> coords,
          std::vector<int> species,
          std::array<std::array<double, 3>, 3> box,
          std::array<bool, 3> periodic,
          std::string label = "");

    /** Return the number of atoms in the frame. */
    size_t size() const noexcept;

    /**
     * Load a single-frame extxyz file.
     *
     * The loader enforces a strict one-frame assumption so the input stays
     * simple and predictable for the project. If @p parse_species is false,
     * element symbols are read and ignored, and the species array is filled
     * with zeros.
     *
     * The extxyz comment line must include the periodic box via a `Lattice=`
     * field. We currently treat the box as orthogonal, so the diagonal terms
     * store the side lengths and the off-diagonal terms are ignored.
     *
     * @param path Path to the extxyz file.
     * @param parse_species Whether to convert element symbols into integer labels.
     */
    void load_from_extxyz(const std::string &path, bool parse_species = false);

    /**
     * Return the squared distance between atoms i and j.
     *
     * Periodic dimensions use the minimum-image convention, which wraps the
     * displacement back into the closest periodic image before computing the
     * squared norm.
     *
     * This helper is kept for clarity and for potential reuse in later code,
     * but the current serial neighbor-list builder inlines the same arithmetic
     * in its hot loop to avoid per-pair function-call overhead.
     */
    double distance2(size_t i, size_t j) const;

    /** Read-only access to stored coordinates. */
    const std::vector<coord_t> &coords_ref() const noexcept { return coords_; }
    /** Read-only access to stored species labels. */
    const std::vector<int> &species_ref() const noexcept { return species_; }
    /** Read-only access to the box matrix. */
    const std::array<std::array<double, 3>, 3> &box_ref() const noexcept { return box_; }
    /** Read-only access to periodicity flags. */
    const std::array<bool, 3> &periodicity() const noexcept { return periodic_; }
    /** Read-only access to the optional frame label. */
    const std::string &label() const noexcept { return label_; }

    /** Set the box matrix after loading coordinates. */
    void set_box(const std::array<std::array<double, 3>, 3> &box);
    /** Set the periodicity flags after loading coordinates. */
    void set_periodic(const std::array<bool, 3> &p);

private:
    std::vector<coord_t> coords_;
    std::vector<int> species_;
    /**
     * Periodic box description.
     *
     * For the current project we treat the box as orthogonal, so the diagonal
     * entries store the box lengths `(Lx, Ly, Lz)` and the off-diagonal terms
     * are ignored. In the more general crystallographic form, the three box
     * vectors would define the periodic lattice:
     *
     *   r + n1 * a + n2 * b + n3 * c,  where n1, n2, n3 are integers.
     *
     * Our current minimum-image calculation only uses the orthogonal case.
     */
    std::array<std::array<double, 3>, 3> box_{};
    std::array<bool, 3> periodic_{{false, false, false}};
    std::string label_;
};

#endif // CS2050_FRAME_H
