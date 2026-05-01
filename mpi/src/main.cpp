#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <mpi.h>
#include <omp.h>

#include "edge_list.h"
#include "frame.h"
#include "timing.h"

/** Minimal MPI-facing atom record: global ID plus coordinates. */
struct Atom
{
    int global_id;
    double x;
    double y;
    double z;
};

/** Build and commit an MPI datatype matching the Atom struct layout. */
static MPI_Datatype create_mpi_atom_type()
{
    MPI_Datatype mpi_atom_type = MPI_DATATYPE_NULL;

    int block_lengths[2] = {1, 3};
    MPI_Datatype types[2] = {MPI_INT, MPI_DOUBLE};
    MPI_Aint offsets[2] = {
        offsetof(Atom, global_id),
        offsetof(Atom, x),
    };

    MPI_Type_create_struct(2, block_lengths, offsets, types, &mpi_atom_type);
    MPI_Type_commit(&mpi_atom_type);
    return mpi_atom_type;
}

/** Wrap a periodic coordinate into [0, L) or clamp a nonperiodic one. */
static double canonicalize_coordinate(double value, double box_length, bool periodic)
{
    if (box_length <= 0.0)
    {
        return value;
    }

    if (periodic)
    {
        value -= std::floor(value / box_length) * box_length;
        if (value >= box_length)
        {
            value = std::nextafter(box_length, 0.0);
        }
        return value;
    }

    if (value < 0.0)
    {
        return 0.0;
    }
    if (value >= box_length)
    {
        return std::nextafter(box_length, 0.0);
    }
    return value;
}

/** Compute the Cartesian-grid owner rank for one atom position. */
static int owner_rank_for_atom(const Atom &atom,
                               const int dims[3],
                               const int periods[3],
                               const std::array<double, 9> &box_flat,
                               MPI_Comm cart_comm)
{
    const double lengths[3] = {box_flat[0], box_flat[4], box_flat[8]};
    const double coords_xyz[3] = {atom.x, atom.y, atom.z};
    int owner_coords[3] = {0, 0, 0};

    for (int d = 0; d < 3; ++d)
    {
        const double coord = canonicalize_coordinate(coords_xyz[d], lengths[d], periods[d] != 0);
        const double scaled = (lengths[d] > 0.0) ? (coord / lengths[d]) : 0.0;
        int cell = static_cast<int>(scaled * dims[d]);
        if (cell < 0)
        {
            cell = 0;
        }
        if (cell >= dims[d])
        {
            cell = dims[d] - 1;
        }
        owner_coords[d] = cell;
    }

    int owner_rank = -1;
    MPI_Cart_rank(cart_comm, owner_coords, &owner_rank);
    return owner_rank;
}

struct SubdomainBounds
{
    std::array<double, 3> lo;
    std::array<double, 3> hi;
};

constexpr int TAG_OWNED_COUNT = 100;
constexpr int TAG_OWNED_ATOMS = 101;
constexpr int TAG_X_LOW_COUNT = 200;
constexpr int TAG_X_LOW_ATOMS = 201;
constexpr int TAG_X_HIGH_COUNT = 202;
constexpr int TAG_X_HIGH_ATOMS = 203;
constexpr int TAG_Y_LOW_COUNT = 210;
constexpr int TAG_Y_LOW_ATOMS = 211;
constexpr int TAG_Y_HIGH_COUNT = 212;
constexpr int TAG_Y_HIGH_ATOMS = 213;
constexpr int TAG_Z_LOW_COUNT = 220;
constexpr int TAG_Z_LOW_ATOMS = 221;
constexpr int TAG_Z_HIGH_COUNT = 222;
constexpr int TAG_Z_HIGH_ATOMS = 223;

static SubdomainBounds subdomain_bounds_for_rank(const int coords[3],
                                                 const int dims[3],
                                                 const std::array<double, 9> &box_flat)
{
    const double lengths[3] = {box_flat[0], box_flat[4], box_flat[8]};
    SubdomainBounds bounds{};
    for (int d = 0; d < 3; ++d)
    {
        const double width = lengths[d] / static_cast<double>(dims[d]);
        bounds.lo[d] = coords[d] * width;
        bounds.hi[d] = (coords[d] + 1) * width;
    }
    return bounds;
}

static std::vector<Atom> gather_visible_atoms(const std::vector<Atom> &owned_atoms,
                                              const std::vector<Atom> &halo_atoms)
{
    std::vector<Atom> visible_atoms;
    visible_atoms.reserve(owned_atoms.size() + halo_atoms.size());
    visible_atoms.insert(visible_atoms.end(), owned_atoms.begin(), owned_atoms.end());
    visible_atoms.insert(visible_atoms.end(), halo_atoms.begin(), halo_atoms.end());
    return visible_atoms;
}

static std::vector<Atom> collect_boundary_atoms(const std::vector<Atom> &visible_atoms,
                                                const SubdomainBounds &bounds,
                                                const std::array<double, 9> &box_flat,
                                                const int periods[3],
                                                int axis,
                                                bool low_side,
                                                double cutoff)
{
    const double box_length = (axis == 0) ? box_flat[0] : (axis == 1) ? box_flat[4] : box_flat[8];
    std::vector<Atom> boundary_atoms;
    for (const auto &atom : visible_atoms)
    {
        const double raw = (axis == 0) ? atom.x : (axis == 1) ? atom.y : atom.z;
        const double coord = canonicalize_coordinate(raw, box_length, periods[axis] != 0);
        const double distance_to_face = low_side ? (coord - bounds.lo[axis]) : (bounds.hi[axis] - coord);
        if (distance_to_face < cutoff)
        {
            boundary_atoms.push_back(atom);
        }
    }
    return boundary_atoms;
}

static std::vector<Atom> merge_unique_atoms(const std::vector<Atom> &a,
                                            const std::vector<Atom> &b)
{
    std::vector<Atom> merged;
    merged.reserve(a.size() + b.size());
    merged.insert(merged.end(), a.begin(), a.end());
    merged.insert(merged.end(), b.begin(), b.end());
    std::sort(merged.begin(), merged.end(),
              [](const Atom &lhs, const Atom &rhs) { return lhs.global_id < rhs.global_id; });
    merged.erase(
        std::unique(merged.begin(), merged.end(),
                    [](const Atom &lhs, const Atom &rhs) { return lhs.global_id == rhs.global_id; }),
        merged.end());
    return merged;
}

static void exchange_one_boundary(const std::vector<Atom> &send_atoms,
                                  int send_to_rank,
                                  int recv_from_rank,
                                  int count_tag,
                                  int atoms_tag,
                                  MPI_Datatype mpi_atom_type,
                                  MPI_Comm comm,
                                  std::vector<Atom> &received_atoms)
{
    received_atoms.clear();

    if (send_to_rank == recv_from_rank && send_to_rank == MPI_PROC_NULL)
    {
        return;
    }

    int send_count = static_cast<int>(send_atoms.size());
    int recv_count = 0;

    MPI_Sendrecv(&send_count, 1, MPI_INT, send_to_rank, count_tag,
                 &recv_count, 1, MPI_INT, recv_from_rank, count_tag,
                 comm, MPI_STATUS_IGNORE);

    received_atoms.resize(recv_count);
    MPI_Sendrecv(send_atoms.empty() ? nullptr : send_atoms.data(), send_count, mpi_atom_type, send_to_rank, atoms_tag,
                 received_atoms.empty() ? nullptr : received_atoms.data(), recv_count, mpi_atom_type, recv_from_rank, atoms_tag,
                 comm, MPI_STATUS_IGNORE);
}

static void exchange_axis_halo(const std::vector<Atom> &owned_atoms,
                               std::vector<Atom> &halo_atoms,
                               const SubdomainBounds &bounds,
                               const std::array<double, 9> &box_flat,
                               const int dims[3],
                               const int periods[3],
                               int axis,
                               double cutoff,
                               MPI_Datatype mpi_atom_type,
                               MPI_Comm cart_comm)
{
    if (dims[axis] == 1)
    {
        return;
    }

    int neighbor_minus = MPI_PROC_NULL;
    int neighbor_plus = MPI_PROC_NULL;
    MPI_Cart_shift(cart_comm, axis, 1, &neighbor_minus, &neighbor_plus);

    const auto visible_atoms = gather_visible_atoms(owned_atoms, halo_atoms);
    const auto send_low = collect_boundary_atoms(visible_atoms, bounds, box_flat, periods, axis, true, cutoff);
    const auto send_high = collect_boundary_atoms(visible_atoms, bounds, box_flat, periods, axis, false, cutoff);

    std::vector<Atom> recv_high_from_minus;
    std::vector<Atom> recv_low_from_plus;

    const int count_tags[3][2] = {
        {TAG_X_LOW_COUNT, TAG_X_HIGH_COUNT},
        {TAG_Y_LOW_COUNT, TAG_Y_HIGH_COUNT},
        {TAG_Z_LOW_COUNT, TAG_Z_HIGH_COUNT},
    };
    const int atom_tags[3][2] = {
        {TAG_X_LOW_ATOMS, TAG_X_HIGH_ATOMS},
        {TAG_Y_LOW_ATOMS, TAG_Y_HIGH_ATOMS},
        {TAG_Z_LOW_ATOMS, TAG_Z_HIGH_ATOMS},
    };

    const int self_rank = [](MPI_Comm comm) {
        int r = -1;
        MPI_Comm_rank(comm, &r);
        return r;
    }(cart_comm);

    if (neighbor_minus == neighbor_plus)
    {
        if (neighbor_minus == self_rank)
        {
            return;
        }

        const auto send_union = merge_unique_atoms(send_low, send_high);
        std::vector<Atom> recv_union;
        exchange_one_boundary(send_union, neighbor_minus, neighbor_minus,
                              count_tags[axis][0], atom_tags[axis][0],
                              mpi_atom_type, cart_comm, recv_union);
        halo_atoms.insert(halo_atoms.end(), recv_union.begin(), recv_union.end());
        return;
    }

    if (neighbor_minus != self_rank)
    {
        exchange_one_boundary(send_high, neighbor_plus, neighbor_minus,
                              count_tags[axis][1], atom_tags[axis][1],
                              mpi_atom_type, cart_comm, recv_high_from_minus);
    }
    if (neighbor_plus != self_rank)
    {
        exchange_one_boundary(send_low, neighbor_minus, neighbor_plus,
                              count_tags[axis][0], atom_tags[axis][0],
                              mpi_atom_type, cart_comm, recv_low_from_plus);
    }

    halo_atoms.insert(halo_atoms.end(), recv_low_from_plus.begin(), recv_low_from_plus.end());
    halo_atoms.insert(halo_atoms.end(), recv_high_from_minus.begin(), recv_high_from_minus.end());
}

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

/** Squared minimum-image distance between two MPI-local atoms. */
static double atom_distance2(const Atom &a,
                             const Atom &b,
                             const std::array<double, 9> &box_flat,
                             const int periods[3])
{
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double dz = b.z - a.z;

    if (periods[0] != 0)
    {
        const double Lx = box_flat[0];
        if (Lx > 0.0)
        {
            if (dx > 0.5 * Lx)
                dx -= Lx;
            else if (dx < -0.5 * Lx)
                dx += Lx;
        }
    }
    if (periods[1] != 0)
    {
        const double Ly = box_flat[4];
        if (Ly > 0.0)
        {
            if (dy > 0.5 * Ly)
                dy -= Ly;
            else if (dy < -0.5 * Ly)
                dy += Ly;
        }
    }
    if (periods[2] != 0)
    {
        const double Lz = box_flat[8];
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
 * Build a half neighbor list with an OpenMP-parallel outer loop.
 *
 * In the MPI version, this will be the local shared-memory kernel that runs on
 * each rank after ownership and halo setup are complete.
 */
static void build_local_edges_openmp(const std::vector<Atom> &owned_atoms,
                                     const std::vector<Atom> &halo_atoms,
                                     const std::array<double, 9> &box_flat,
                                     const int periods[3],
                                     double cutoff,
                                     EdgeList &out)
{
    out.clear();
    const double cutoff2 = cutoff * cutoff;
    const std::size_t N_owned = owned_atoms.size();

    std::vector<std::vector<EdgeList::idx_t>> per_source_destinations(N_owned);

#pragma omp parallel for schedule(dynamic)
    for (std::size_t i = 0; i < N_owned; ++i)
    {
        const Atom &source = owned_atoms[i];
        auto &destinations = per_source_destinations[i];
        destinations.reserve(64);

        for (std::size_t j = i + 1; j < N_owned; ++j)
        {
            const Atom &dest = owned_atoms[j];
            if (source.global_id < dest.global_id &&
                atom_distance2(source, dest, box_flat, periods) <= cutoff2)
            {
                destinations.push_back(static_cast<EdgeList::idx_t>(dest.global_id));
            }
        }

        for (const auto &dest : halo_atoms)
        {
            if (source.global_id < dest.global_id &&
                atom_distance2(source, dest, box_flat, periods) <= cutoff2)
            {
                destinations.push_back(static_cast<EdgeList::idx_t>(dest.global_id));
            }
        }

        std::sort(destinations.begin(), destinations.end());
    }

    std::size_t total_edges = 0;
    for (const auto &destinations : per_source_destinations)
    {
        total_edges += destinations.size();
    }
    out.reserve(total_edges);

    for (std::size_t i = 0; i < N_owned; ++i)
    {
        const auto &destinations = per_source_destinations[i];
        for (const auto j : destinations)
        {
            out.sources.push_back(static_cast<EdgeList::idx_t>(owned_atoms[i].global_id));
            out.destinations.push_back(j);
        }
    }
}

static void sort_edge_list_pairs(EdgeList &edges)
{
    std::vector<std::size_t> order(edges.size());
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        order[i] = i;
    }

    std::sort(order.begin(), order.end(),
              [&edges](std::size_t a, std::size_t b)
              {
                  if (edges.sources[a] != edges.sources[b])
                  {
                      return edges.sources[a] < edges.sources[b];
                  }
                  return edges.destinations[a] < edges.destinations[b];
              });

    std::vector<EdgeList::idx_t> sorted_sources;
    std::vector<EdgeList::idx_t> sorted_destinations;
    sorted_sources.reserve(edges.size());
    sorted_destinations.reserve(edges.size());
    for (const auto idx : order)
    {
        sorted_sources.push_back(edges.sources[idx]);
        sorted_destinations.push_back(edges.destinations[idx]);
    }
    edges.sources = std::move(sorted_sources);
    edges.destinations = std::move(sorted_destinations);
}

static EdgeList gather_edges_to_root(const EdgeList &local_edges, int world_rank, int world_size)
{
    const int local_count = static_cast<int>(local_edges.size());
    std::vector<int> counts;
    if (world_rank == 0)
    {
        counts.resize(world_size);
    }

    MPI_Gather(&local_count, 1, MPI_INT,
               counts.empty() ? nullptr : counts.data(), 1, MPI_INT,
               0, MPI_COMM_WORLD);

    EdgeList gathered;
    std::vector<int> displs;
    int total_count = 0;
    if (world_rank == 0)
    {
        displs.resize(world_size, 0);
        for (int r = 0; r < world_size; ++r)
        {
            displs[r] = total_count;
            total_count += counts[r];
        }
        gathered.sources.resize(total_count);
        gathered.destinations.resize(total_count);
    }

    MPI_Gatherv(local_edges.sources.empty() ? nullptr : local_edges.sources.data(),
                local_count, MPI_INT64_T,
                gathered.sources.empty() ? nullptr : gathered.sources.data(),
                counts.empty() ? nullptr : counts.data(),
                displs.empty() ? nullptr : displs.data(),
                MPI_INT64_T, 0, MPI_COMM_WORLD);

    MPI_Gatherv(local_edges.destinations.empty() ? nullptr : local_edges.destinations.data(),
                local_count, MPI_INT64_T,
                gathered.destinations.empty() ? nullptr : gathered.destinations.data(),
                counts.empty() ? nullptr : counts.data(),
                displs.empty() ? nullptr : displs.data(),
                MPI_INT64_T, 0, MPI_COMM_WORLD);

    if (world_rank == 0)
    {
        sort_edge_list_pairs(gathered);
    }

    return gathered;
}

/** MPI entry point. We will build the MPI-specific orchestration here next. */
int main(int argc, char **argv)
{
    if (argc < 4 || argc > 7)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <input.extxyz> <output.csv|-> <cutoff> [--timing] [--no-write] [--gather-write]\n";
        return 1;
    }

    const std::string input_path = argv[1];
    const std::string output_path = argv[2];
    const double cutoff = std::stod(argv[3]);
    bool print_timing = false;
    bool skip_write = (output_path == "-");
    bool gather_write = false;
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
        else if (flag == "--gather-write")
        {
            gather_write = true;
        }
        else
        {
            std::cerr << "Unknown flag: " << flag << '\n';
            return 1;
        }
    }

    (void)cutoff;
    (void)print_timing;

    // Start the MPI runtime. Every MPI process must call MPI_Init before using
    // other MPI routines.
    MPI_Init(&argc, &argv);

    int world_rank = 0;
    int world_size = 0;

    // Query this process's rank in MPI_COMM_WORLD, the default communicator
    // containing every launched MPI process.
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // Query how many total processes are participating in MPI_COMM_WORLD.
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    TimingSummary timing;
    Frame full_frame;
    std::array<double, 9> box_flat = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<int, 3> periodic_ints = {0, 0, 0};
    int load_ok = 1;

    // Rank 0 reads the extxyz file once and broadcasts only the metadata
    // needed to construct the Cartesian topology on every rank.
    if (world_rank == 0)
    {
        try
        {
            ScopedTimer timer(timing.load_ms);
            full_frame.load_from_extxyz(input_path, false);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to load frame: " << e.what() << '\n';
            load_ok = 0;
        }

        if (load_ok)
        {
            const auto &box = full_frame.box_ref();
            const auto &periodic = full_frame.periodicity();
            for (int r = 0; r < 3; ++r)
            {
                for (int c = 0; c < 3; ++c)
                {
                    box_flat[3 * r + c] = box[r][c];
                }
                periodic_ints[r] = periodic[r] ? 1 : 0;
            }
        }
    }

    MPI_Bcast(&load_ok, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (!load_ok)
    {
        MPI_Finalize();
        return 1;
    }

    MPI_Bcast(box_flat.data(), static_cast<int>(box_flat.size()), MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(periodic_ints.data(), static_cast<int>(periodic_ints.size()), MPI_INT, 0, MPI_COMM_WORLD);

    int dims[3] = {0, 0, 0};

    // Ask MPI to choose a balanced 3D process grid for the available number of
    // ranks. For example, 8 ranks becomes 2 x 2 x 2.
    MPI_Dims_create(world_size, 3, dims);

    int periods[3] = {periodic_ints[0], periodic_ints[1], periodic_ints[2]};

    // Build a Cartesian communicator on top of MPI_COMM_WORLD. The new
    // communicator stores a logical 3D grid topology for the ranks.
    MPI_Comm cart_comm = MPI_COMM_NULL;
    MPI_Cart_create(MPI_COMM_WORLD, 3, dims, periods, 0, &cart_comm);

    int cart_rank = -1;
    int coords[3] = {-1, -1, -1}; // will be the cordinate id of the rank

    // A Cartesian communicator may reorder ranks if requested. We set reorder=0
    // above, but still query the rank and coordinates from the communicator we
    // will actually use downstream.
    MPI_Comm_rank(cart_comm, &cart_rank);
    MPI_Cart_coords(cart_comm, cart_rank, 3, coords);

    MPI_Datatype mpi_atom_type = create_mpi_atom_type();

    const SubdomainBounds bounds = subdomain_bounds_for_rank(coords, dims, box_flat);

    // These are the rank-local buffers we populate through ownership
    // distribution and halo exchange.
    std::vector<Atom> owned_atoms;
    std::vector<Atom> halo_atoms;

    // First ownership distribution step:
    // rank 0 walks the loaded frame once, computes each atom's home rank from
    // its position and the Cartesian grid, and sends each rank its owned atoms.
    if (world_rank == 0)
    {
        const auto &coords_ref = full_frame.coords_ref();
        std::vector<std::vector<Atom>> send_buffers(world_size);
        for (std::size_t i = 0; i < coords_ref.size(); ++i)
        {
            Atom atom{
                static_cast<int>(i),
                coords_ref[i][0],
                coords_ref[i][1],
                coords_ref[i][2],
            };
            const int owner = owner_rank_for_atom(atom, dims, periods, box_flat, cart_comm);
            send_buffers[owner].push_back(atom);
        }

        for (int dest = 1; dest < world_size; ++dest)
        {
            const int count = static_cast<int>(send_buffers[dest].size());
            MPI_Send(&count, 1, MPI_INT, dest, TAG_OWNED_COUNT, MPI_COMM_WORLD);
            if (count > 0)
            {
                MPI_Send(send_buffers[dest].data(), count, mpi_atom_type, dest, TAG_OWNED_ATOMS, MPI_COMM_WORLD);
            }
        }
        owned_atoms = std::move(send_buffers[0]);
    }
    else
    {
        int count = 0;
        MPI_Recv(&count, 1, MPI_INT, 0, TAG_OWNED_COUNT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        owned_atoms.resize(count);
        if (count > 0)
        {
            MPI_Recv(owned_atoms.data(), count, mpi_atom_type, 0, TAG_OWNED_ATOMS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }

    std::sort(owned_atoms.begin(), owned_atoms.end(),
              [](const Atom &a, const Atom &b) { return a.global_id < b.global_id; });

    exchange_axis_halo(owned_atoms, halo_atoms, bounds, box_flat, dims, periods, 0, cutoff, mpi_atom_type, cart_comm);
    exchange_axis_halo(owned_atoms, halo_atoms, bounds, box_flat, dims, periods, 1, cutoff, mpi_atom_type, cart_comm);
    exchange_axis_halo(owned_atoms, halo_atoms, bounds, box_flat, dims, periods, 2, cutoff, mpi_atom_type, cart_comm);

    EdgeList local_edges;
    {
        ScopedTimer timer(timing.build_ms);
        build_local_edges_openmp(owned_atoms, halo_atoms, box_flat, periods, cutoff, local_edges);
    }

    long long local_edge_count = static_cast<long long>(local_edges.size());
    long long global_edge_count = 0;
    MPI_Reduce(&local_edge_count, &global_edge_count, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    const bool do_gather_write = gather_write && !skip_write;
    EdgeList gathered_edges;
    if (do_gather_write)
    {
        {
            ScopedTimer timer(timing.write_ms);
            gathered_edges = gather_edges_to_root(local_edges, world_rank, world_size);
        }
        if (world_rank == 0)
        {
            write_edges_csv(gathered_edges, output_path);
        }
    }

    std::cout << "World rank " << world_rank
              << " / " << world_size
              << " -> cart rank " << cart_rank
              << " coords=(" << coords[0] << ", " << coords[1] << ", " << coords[2] << ")"
              << " dims=(" << dims[0] << ", " << dims[1] << ", " << dims[2] << ")"
              << " pbc=(" << periods[0] << ", " << periods[1] << ", " << periods[2] << ")"
              << " owned=" << owned_atoms.size()
              << " halo=" << halo_atoms.size()
              << " local_edges=" << local_edges.size()
              << " threads=" << omp_get_max_threads() << '\n';

    if (world_rank == 0)
    {
        std::cout << "Global edge count: " << global_edge_count << " (cutoff=" << cutoff << ")\n";
        if (do_gather_write)
        {
            std::cout << "Gathered and wrote " << gathered_edges.size() << " edges to " << output_path << '\n';
        }
    }

    MPI_Type_free(&mpi_atom_type);
    MPI_Comm_free(&cart_comm);

    // Shut down the MPI runtime. This should be the last MPI call in main.
    MPI_Finalize();

    return 0;
}
