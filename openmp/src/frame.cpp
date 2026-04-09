#include "frame.h"

#include <fstream>
#include <sstream>
#include <cmath>
#include <stdexcept>

Frame::Frame(std::vector<coord_t> coords,
             std::vector<int> species,
             std::array<std::array<double, 3>, 3> box,
             std::array<bool, 3> periodic,
             std::string label)
    : coords_(std::move(coords)), species_(std::move(species)), box_(box), periodic_(periodic), label_(std::move(label))
{
    if (coords_.size() != species_.size()) {
        throw std::invalid_argument("coords and species must have same length");
    }
}

size_t Frame::size() const noexcept {
    return coords_.size();
}

void Frame::load_from_simple_file(const std::string &path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("failed to open " + path);

    coords_.clear();
    species_.clear();

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        // skip comments
        char c = ss.peek();
        if (c == '#') continue;

        double x,y,z; int sp;
        for (char &ch : line) if (ch == ',') ch = ' ';
        std::istringstream ss2(line);
        if (!(ss2 >> x >> y >> z >> sp)) continue;

        coords_.push_back(coord_t{x,y,z});
        species_.push_back(sp);
    }

    if (coords_.size() != species_.size()) {
        throw std::runtime_error("inconsistent data read from " + path);
    }
}

Frame::coord_t Frame::minimum_image_disp(size_t i, size_t j) const {
    if (i >= coords_.size() || j >= coords_.size()) throw std::out_of_range("index");
    coord_t d;
    for (int k=0;k<3;++k) d[k] = coords_[j][k] - coords_[i][k];
    for (int k=0;k<3;++k) {
        if (periodic_[k]) {
            double L = box_[k][k];
            if (L > 0.0) d[k] -= std::round(d[k] / L) * L;
        }
    }
    return d;
}

double Frame::distance(size_t i, size_t j) const {
    coord_t d = minimum_image_disp(i,j);
    return std::sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
}
