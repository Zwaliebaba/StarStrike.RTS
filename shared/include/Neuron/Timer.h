#pragma once

#include <chrono>
#include <cstdint>

namespace Neuron
{

/// High-resolution timer for frame timing and tick measurement.
class Timer
{
public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration  = Clock::duration;

    Timer() : m_start(Clock::now()), m_last(m_start) {}

    /// Reset the timer origin to now.
    void reset() noexcept
    {
        m_start = Clock::now();
        m_last  = m_start;
    }

    /// Advance the timer: returns seconds elapsed since last call to tick().
    [[nodiscard]] float tick() noexcept
    {
        auto now = Clock::now();
        auto dt  = std::chrono::duration<float>(now - m_last).count();
        m_last   = now;
        return dt;
    }

    /// Seconds elapsed since construction or last reset().
    [[nodiscard]] float elapsedSec() const noexcept
    {
        return std::chrono::duration<float>(Clock::now() - m_start).count();
    }

    /// Microseconds elapsed since construction or last reset().
    [[nodiscard]] int64_t elapsedUs() const noexcept
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            Clock::now() - m_start).count();
    }

private:
    TimePoint m_start;
    TimePoint m_last;
};

} // namespace Neuron
