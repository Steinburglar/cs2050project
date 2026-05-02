#!/usr/bin/env julia

struct Frame
    # SoA: one array per coordinate axis.
    x::Vector{Float64}
    y::Vector{Float64}
    z::Vector{Float64}
    box::NTuple{3,Float64}
    periodic::NTuple{3,Bool}
end

struct EdgeList
    # Same SoA idea as C++ edge list.
    sources::Vector{Int}
    destinations::Vector{Int}
end

function EdgeList()::EdgeList
    return EdgeList(Int[], Int[])
end

function push_edge!(edges::EdgeList, source::Int, destination::Int)
    push!(edges.sources, source)
    push!(edges.destinations, destination)
end

function write_edges_csv(edges::EdgeList, path::String)
    open(path, "w") do io
        for k in eachindex(edges.sources)
            println(io, edges.sources[k], ",", edges.destinations[k])
        end
    end
end

function minimum_image_distance2(frame::Frame, i::Int, j::Int)::Float64
    dx = frame.x[j] - frame.x[i]
    dy = frame.y[j] - frame.y[i]
    dz = frame.z[j] - frame.z[i]

    if frame.periodic[1]
        Lx = frame.box[1]
        if dx > 0.5 * Lx
            dx -= Lx
        elseif dx < -0.5 * Lx
            dx += Lx
        end
    end
    if frame.periodic[2]
        Ly = frame.box[2]
        if dy > 0.5 * Ly
            dy -= Ly
        elseif dy < -0.5 * Ly
            dy += Ly
        end
    end
    if frame.periodic[3]
        Lz = frame.box[3]
        if dz > 0.5 * Lz
            dz -= Lz
        elseif dz < -0.5 * Lz
            dz += Lz
        end
    end

    return dx * dx + dy * dy + dz * dz
end

function build_edges(frame::Frame, cutoff::Float64)::EdgeList
    cutoff2 = cutoff * cutoff
    n = length(frame.x)
    per_source = [EdgeList() for _ in 1:n]

    Threads.@threads for i in 1:n
        local_edges = per_source[i]
        for j in (i+1):length(frame.x)
            if minimum_image_distance2(frame, i, j) <= cutoff2
                push_edge!(local_edges, i - 1, j - 1) # to match 0 index standard of csv output
            end
        end
    end

    edges = EdgeList()
    for local_edges in per_source
        append!(edges.sources, local_edges.sources)
        append!(edges.destinations, local_edges.destinations)
    end

    return edges
end

function load_extxyz(path::String)::Frame
    # `path::String` means: argument named `path`, and Julia expects it to be a `String`.
    lines = readlines(path)
    # `readlines` returns array of `String`, one per line.
    if length(lines) < 2
        # `length(x)` gives element count.
        error("extxyz file too short: $path")
    end

    # `parse(Int, text)` convert text to integer.
    atom_count = parse(Int, strip(lines[1]))
    # `strip` remove leading/trailing whitespace.
    if length(lines) < atom_count + 2
        error("extxyz file missing atom lines: $path")
    end

    # Second line has metadata like Lattice= and pbc=.
    header = lines[2]
    box = (0.0, 0.0, 0.0)
    periodic = (false, false, false)

    # Parse `Lattice=` from header.
    lattice_match = match(r"Lattice=\"([^\"]+)\"", header)
    if lattice_match !== nothing
        lattice_vals = split(lattice_match.captures[1])
        if length(lattice_vals) >= 9
            # Orthogonal box: take diagonal entries.
            box = (
                parse(Float64, lattice_vals[1]),
                parse(Float64, lattice_vals[5]),
                parse(Float64, lattice_vals[9]),
            )
        end
    end

    # Parse `pbc=` from header.
    pbc_match = match(r"pbc=\"([^\"]+)\"", header)
    if pbc_match !== nothing
        pbc_vals = split(pbc_match.captures[1])
        if length(pbc_vals) >= 3
            periodic = (
                lowercase(pbc_vals[1]) == "t" || lowercase(pbc_vals[1]) == "true",
                lowercase(pbc_vals[2]) == "t" || lowercase(pbc_vals[2]) == "true",
                lowercase(pbc_vals[3]) == "t" || lowercase(pbc_vals[3]) == "true",
            )
        end
    end

    # `Float64[]` make empty vector of Float64.
    # SoA coordinate arrays: one vector per axis.
    x = Float64[]
    y = Float64[]
    z = Float64[]
    # `for line in lines[3:(atom_count + 2)]` loop over slice of lines.
    # `3:(atom_count + 2)` is Julia range syntax.
    for line in lines[3:(atom_count+2)]
        # `split` break string into array of substrings by whitespace.
        fields = split(strip(line))
        if length(fields) < 4
            error("bad atom line: $line")
        end
        # `fields[2]` = second token. Julia arrays are 1-based, not 0-based.
        push!(x, parse(Float64, fields[2]))
        push!(y, parse(Float64, fields[3]))
        push!(z, parse(Float64, fields[4]))
        # `push!` append value to end of vector.
    end

    # Build `Frame` object from parsed coordinates.
    return Frame(x, y, z, box, periodic)
end

function main()
    # `ARGS` is built-in array of command-line args after script name.
    if length(ARGS) < 3
        println("Usage: julia main.jl <input.extxyz> <output.csv|-> <cutoff> [--timing] [--no-write]")
        return 1
    end

    # `ARGS[1]` is first CLI arg. Julia arrays start at 1.
    input_path = ARGS[1]
    output_path = ARGS[2]
    cutoff = parse(Float64, ARGS[3])
    print_timing = false
    skip_write = output_path == "-"

    for arg in ARGS[4:end]
        if arg == "--timing"
            print_timing = true
        elseif arg == "--no-write"
            skip_write = true
        else
            error("Unknown flag: $arg")
        end
    end

    t_load = 0.0
    t_build = 0.0
    t_write = 0.0

    # Thread check.
    println("Julia threads: ", Threads.nthreads())

    # Call another function with parentheses like normal.
    t_load = @elapsed frame = load_extxyz(input_path)
    t_build = @elapsed edges = build_edges(frame, cutoff)

    println("Julia run")
    println("Input: ", input_path)
    println("Output: ", skip_write ? "<disabled>" : output_path)
    println("Cutoff: ", cutoff)
    println("Atoms: ", length(frame.x))
    println("Box: ", frame.box)
    println("Periodic: ", frame.periodic)
    println("Edges: ", length(edges.sources))
    if !skip_write
        t_write = @elapsed write_edges_csv(edges, output_path)
    end
    if print_timing
        println("Timing summary (ms): load=", t_load * 1000, " build=", t_build * 1000, " write=", t_write * 1000, " total=", (t_load + t_build + t_write) * 1000)
    end
    # `println` print value plus newline.
    println("Args: ", ARGS)
    return 0
end

# `exit(main())` makes script exit with returned status code.
exit(main())
