#include "pch.h"
#include "TickProfiler.h"

#include "ServerLog.h"

namespace Neuron::Server
{

void TickProfiler::printHistogram() const
{
    if (m_samples.empty()) return;

    auto sorted = sortedCopy();
    double minVal = sorted.front();
    double maxVal = sorted.back();
    double p50 = percentile(sorted, 0.50);
    double p95 = percentile(sorted, 0.95);
    double p99 = percentile(sorted, 0.99);

    LogInfo("Tick stats ({} samples): min={:.2f}ms p50={:.2f}ms "
            "p95={:.2f}ms p99={:.2f}ms max={:.2f}ms\n",
            sorted.size(), minVal, p50, p95, p99, maxVal);

    if (p99 > 16.67)
        LogWarn("Tick p99 ({:.2f}ms) exceeds 16.67ms budget!\n", p99);
}

} // namespace Neuron::Server
