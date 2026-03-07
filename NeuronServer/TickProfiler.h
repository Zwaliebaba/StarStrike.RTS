#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <numeric>

namespace Neuron::Server
{

/// Records per-tick timing and computes rolling histograms.
/// Prints min/p50/p95/p99/max every `printInterval` ticks.
class TickProfiler
{
public:
    static constexpr uint32_t WINDOW_SIZE     = 600;  // 10 sec @ 60 Hz
    static constexpr uint32_t PRINT_INTERVAL  = 60;   // every 1 sec

    using Clock = std::chrono::steady_clock;

    void beginTick() { m_tickStart = Clock::now(); }

    void endTick()
    {
        auto elapsed = Clock::now() - m_tickStart;
        double ms = std::chrono::duration<double, std::milli>(elapsed).count();

        m_samples.push_back(ms);
        if (m_samples.size() > WINDOW_SIZE)
            m_samples.pop_front();

        ++m_ticksSincePrint;
        if (m_ticksSincePrint >= PRINT_INTERVAL)
        {
            printHistogram();
            m_ticksSincePrint = 0;
        }
    }

    /// Returns true if p99 over the last WINDOW_SIZE ticks is under 16.67 ms.
    [[nodiscard]] bool isHealthy() const
    {
        if (m_samples.empty()) return true;
        auto sorted = sortedCopy();
        double p99 = percentile(sorted, 0.99);
        return p99 < 16.67;
    }

    void printHistogram() const;

private:
    [[nodiscard]] std::vector<double> sortedCopy() const
    {
        std::vector<double> v(m_samples.begin(), m_samples.end());
        std::ranges::sort(v);
        return v;
    }

    [[nodiscard]] static double percentile(const std::vector<double>& sorted, double p)
    {
        if (sorted.empty()) return 0.0;
        size_t idx = static_cast<size_t>(p * static_cast<double>(sorted.size() - 1));
        return sorted[idx];
    }

    std::deque<double>         m_samples;
    Clock::time_point          m_tickStart;
    uint32_t                   m_ticksSincePrint = 0;
};

} // namespace Neuron::Server
