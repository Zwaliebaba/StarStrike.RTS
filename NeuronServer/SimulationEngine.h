#pragma once

#include <cstdint>
#include <functional>

namespace Neuron::Server
{

/// Skeletal 60 Hz server tick loop with 6 simulation phases (all stubs).
/// Each phase will be filled in during later implementation phases.
class SimulationEngine
{
public:
    SimulationEngine() = default;

    /// Run one simulation tick. Called every 16.67 ms by the server main loop.
    void tick(uint64_t tickNum);

    [[nodiscard]] uint64_t tickCount() const noexcept { return m_tickCount; }

private:
    void phase1_inputProcessing();
    void phase2_voxelUpdate();
    void phase3_physics();
    void phase4_combat();
    void phase5_mining();
    void phase6_effects();

    uint64_t m_tickCount = 0;
};

} // namespace Neuron::Server
