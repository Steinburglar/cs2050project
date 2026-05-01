/**
 * @file timing.h
 * @brief Tiny timing helpers for phase-level measurement in the CUDA executable.
 */
#ifndef CS2050_TIMING_H
#define CS2050_TIMING_H

#include <chrono>
#include <ostream>

template <typename Duration>
double to_milliseconds(Duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

/**
 * Simple container for the main phase timings in the executable.
 */
struct TimingSummary
{
    double load_ms = 0.0;
    double pack_ms = 0.0;
    double upload_ms = 0.0;
    double build_ms = 0.0;
    double write_ms = 0.0;
    double total_ms = 0.0;

    void print(std::ostream &os) const
    {
        os << "Timing summary (ms): "
           << "load=" << load_ms << ' '
           << "pack=" << pack_ms << ' '
           << "upload=" << upload_ms << ' '
           << "build=" << build_ms << ' '
           << "write=" << write_ms << ' '
           << "total=" << total_ms << '\n';
    }
};

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
