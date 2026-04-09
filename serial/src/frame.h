#ifndef CS2050_FRAME_H
#define CS2050_FRAME_H

#include <vector>
#include <array>
#include <string>
#include <cstdint>

class Frame
{
public:
    using coord_t = std::array<double, 3>;

    Frame() = default;
    Frame(std::vector<coord_t> coords,
          std::vector<int> species,
          std::array<std::array<double, 3>, 3> box,
          std::array<bool, 3> periodic,
          std::string label = "");

    size_t size() const noexcept;

    // Simple loader for debugging: expects lines of `x y z species` (whitespace or CSV).
    // Ignores lines starting with '#'. Does not currently parse box/periodicity metadata.
    void load_from_simple_file(const std::string &path);

    // Minimum-image displacement (assumes orthogonal box lengths stored on diagonal of `box`)
    coord_t minimum_image_disp(size_t i, size_t j) const;
    double distance(size_t i, size_t j) const;

    const std::vector<coord_t> &coords_ref() const noexcept { return coords_; }
    const std::vector<int> &species_ref() const noexcept { return species_; }
    const std::array<std::array<double, 3>, 3> &box_ref() const noexcept { return box_; }
    const std::array<bool, 3> &periodicity() const noexcept { return periodic_; }
    const std::string &label() const noexcept { return label_; }

    // Mutators for box/periodicity (useful after loading from simple files)
    void set_box(const std::array<std::array<double, 3>, 3> &box);
    void set_periodic(const std::array<bool, 3> &p);

private:
    std::vector<coord_t> coords_;
    std::vector<int> species_;
    std::array<std::array<double, 3>, 3> box_{}; // box vectors or diagonal lengths
    std::array<bool, 3> periodic_{{false, false, false}};
    std::string label_;
};

#endif // CS2050_FRAME_H
