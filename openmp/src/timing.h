/**
 * @file timing.h
 * @brief Tiny timing helpers for phase-level measurement in the OpenMP executable.
 */
#ifndef CS2050_TIMING_H
#define CS2050_TIMING_H

#include <chrono>
#include <ostream>

/** Convert a steady-clock duration to milliseconds as a double. */
template <typename Duration>
double to_milliseconds(Duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

/**
 * Simple container for the main phase timings in the executable.
 *
 * This struct does not measure time by itself. It only stores named timing
 * results so the main program can report them in a consistent format.
 */
struct TimingSummary
{
    double load_ms = 0.0;
    double build_ms = 0.0;
    double write_ms = 0.0;
    double total_ms = 0.0;

    /** Print a compact one-line summary for logs and later CSV conversion. */
    void print(std::ostream &os) const
    {
        os << "Timing summary (ms): "
           << "load=" << load_ms << ' '
           << "build=" << build_ms << ' '
           << "write=" << write_ms << ' '
           << "total=" << total_ms << '\n';
    }
};

/**
 * Scope-based timer that stores elapsed time in a referenced double.
 *
 * This class is the measurement mechanism. It starts timing at construction
 * and writes the elapsed milliseconds into the referenced field when the scope
 * ends, which is why it is typically paired with a field inside TimingSummary.
 *
 * Usage:
 *   {
 *       ScopedTimer t(some_double);
 *       do_work();
 *   }
 */
class ScopedTimer
{
public:
    using clock_t = std::chrono::steady_clock;

    explicit ScopedTimer(double &target)
        : target_(target), start_(clock_t::now())
    {
    }

    ~ScopedTimer()
    {
        target_ = to_milliseconds(clock_t::now() - start_);
    }

    ScopedTimer(const ScopedTimer &) = delete;
    ScopedTimer &operator=(const ScopedTimer &) = delete;

private:
    double &target_;
    clock_t::time_point start_;
};

#endif // CS2050_TIMING_H
