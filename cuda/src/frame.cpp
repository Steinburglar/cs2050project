#include "frame.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

/** Construct a frame from existing coordinate/species arrays. */
Frame::Frame(std::vector<coord_t> coords,
             std::vector<int> species,
             std::array<std::array<double, 3>, 3> box,
             std::array<bool, 3> periodic,
             std::string label)
    : coords_(std::move(coords)), species_(std::move(species)), box_(box),
      periodic_(periodic), label_(std::move(label))
{
    if (coords_.size() != species_.size())
    {
        throw std::invalid_argument("coords and species must have same length");
    }
}

size_t Frame::size() const noexcept
{
    return coords_.size();
}

namespace
{
std::string trim(const std::string &s)
{
    const auto begin = std::find_if_not(s.begin(), s.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    const auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();
    if (begin >= end)
        return {};
    return std::string(begin, end);
}

int species_token_to_int(const std::string &token)
{
    try
    {
        size_t pos = 0;
        int value = std::stoi(token, &pos);
        if (pos == token.size())
            return value;
    }
    catch (...)
    {
    }

    static const std::unordered_map<std::string, int> kAtomicNumbers = {
        {"H", 1},  {"He", 2}, {"Li", 3}, {"Be", 4}, {"B", 5},  {"C", 6},
        {"N", 7},  {"O", 8},  {"F", 9},  {"Ne", 10}, {"Na", 11}, {"Mg", 12},
        {"Al", 13}, {"Si", 14}, {"P", 15}, {"S", 16}, {"Cl", 17}, {"Ar", 18}};

    auto it = kAtomicNumbers.find(token);
    if (it != kAtomicNumbers.end())
    {
        return it->second;
    }

    return 0;
}

std::optional<std::string> extract_quoted_attribute(const std::string &line, const std::string &key)
{
    const std::string needle = key + "=";
    const std::size_t pos = line.find(needle);
    if (pos == std::string::npos)
        return std::nullopt;

    const std::size_t first_quote = line.find('"', pos + needle.size());
    if (first_quote == std::string::npos)
        return std::nullopt;

    const std::size_t second_quote = line.find('"', first_quote + 1);
    if (second_quote == std::string::npos || second_quote <= first_quote + 1)
        return std::nullopt;

    return line.substr(first_quote + 1, second_quote - first_quote - 1);
}

bool parse_extxyz_bool_token(const std::string &token)
{
    if (token == "T" || token == "t" || token == "1" || token == "true" || token == "True")
        return true;
    if (token == "F" || token == "f" || token == "0" || token == "false" || token == "False")
        return false;
    throw std::runtime_error("invalid extxyz periodicity token: " + token);
}

std::array<std::array<double, 3>, 3> parse_extxyz_lattice(const std::string &value)
{
    std::istringstream ss(value);
    std::array<std::array<double, 3>, 3> box{};
    std::vector<double> vals;
    double x = 0.0;
    while (ss >> x)
        vals.push_back(x);

    if (vals.size() == 3)
    {
        box[0][0] = vals[0];
        box[1][1] = vals[1];
        box[2][2] = vals[2];
        return box;
    }

    if (vals.size() != 9)
        throw std::runtime_error("expected 3 or 9 lattice values in extxyz comment");

    box[0][0] = vals[0];
    box[1][1] = vals[4];
    box[2][2] = vals[8];
    return box;
}

std::array<bool, 3> parse_extxyz_pbc(const std::string &value)
{
    std::istringstream ss(value);
    std::array<bool, 3> periodic{{false, false, false}};
    std::string token;
    for (int k = 0; k < 3; ++k)
    {
        if (!(ss >> token))
            throw std::runtime_error("expected three periodicity values in extxyz comment");
        periodic[k] = parse_extxyz_bool_token(token);
    }
    return periodic;
}
} // namespace

void Frame::load_from_extxyz(const std::string &path, bool parse_species)
{
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("failed to open " + path);

    coords_.clear();
    species_.clear();

    std::string line;
    if (!std::getline(in, line))
        throw std::runtime_error("empty extxyz file: " + path);

    line = trim(line);
    if (line.empty())
        throw std::runtime_error("missing atom count in extxyz file: " + path);

    std::size_t atom_count = 0;
    try
    {
        size_t pos = 0;
        atom_count = static_cast<std::size_t>(std::stoul(line, &pos));
        if (pos != line.size())
            throw std::runtime_error("");
    }
    catch (...)
    {
        throw std::runtime_error("invalid atom count in extxyz file: " + path);
    }

    coords_.reserve(atom_count);
    species_.reserve(atom_count);

    if (!std::getline(in, line))
        throw std::runtime_error("extxyz file ended before comment line: " + path);

    const auto lattice_attr = extract_quoted_attribute(line, "Lattice");
    if (!lattice_attr)
        throw std::runtime_error("extxyz comment line is missing Lattice attribute: " + path);
    box_ = parse_extxyz_lattice(*lattice_attr);

    if (const auto pbc_attr = extract_quoted_attribute(line, "pbc"))
    {
        periodic_ = parse_extxyz_pbc(*pbc_attr);
    }

    for (std::size_t idx = 0; idx < atom_count; ++idx)
    {
        if (!std::getline(in, line))
            throw std::runtime_error("extxyz file ended before all atoms were read: " + path);

        line = trim(line);
        if (line.empty())
        {
            --idx;
            continue;
        }

        std::istringstream ss(line);
        std::string symbol;
        double x = 0.0, y = 0.0, z = 0.0;
        if (!(ss >> symbol >> x >> y >> z))
            throw std::runtime_error("failed to parse extxyz atom line in " + path);

        coords_.push_back(coord_t{x, y, z});
        species_.push_back(parse_species ? species_token_to_int(symbol) : 0);
    }

    while (std::getline(in, line))
    {
        if (!trim(line).empty())
            throw std::runtime_error("extxyz file contains more than one frame: " + path);
    }
}

double Frame::distance2(size_t i, size_t j) const
{
    if (i >= coords_.size() || j >= coords_.size())
        throw std::out_of_range("index");
    coord_t d;
    for (int k = 0; k < 3; ++k)
        d[k] = coords_[j][k] - coords_[i][k];

    for (int k = 0; k < 3; ++k)
    {
        if (periodic_[k])
        {
            double L = box_[k][k];
            if (L > 0.0)
            {
                if (d[k] > 0.5 * L)
                    d[k] -= L;
                else if (d[k] < -0.5 * L)
                    d[k] += L;
            }
        }
    }

    return d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
}

void Frame::set_box(const std::array<std::array<double, 3>, 3> &box)
{
    box_ = box;
}

void Frame::set_periodic(const std::array<bool, 3> &p)
{
    periodic_ = p;
}
