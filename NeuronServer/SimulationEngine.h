#pragma once

#include "PacketTypes.h"

#include <cstdint>
#include <vector>

namespace Neuron::GameLogic { class UniverseManager; }

namespace Neuron::Server
{

/// 60 Hz server simulation tick loop with 6 phases.
/// Phase 1 (input) is implemented; remaining phases are stubs for later.
class SimulationEngine
{
public:
    SimulationEngine() = default;

    /// Bind the simulation to a universe.  Must be called before tick().
    void init(Neuron::GameLogic::UniverseManager* universe);

    /// Enqueue a validated player command for processing in the next tick.
    void enqueueCommand(const CmdInput& cmd);

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

    Neuron::GameLogic::UniverseManager* m_universe = nullptr;
    std::vector<CmdInput>               m_pendingCommands;
    uint64_t                            m_tickCount = 0;
};

} // namespace Neuron::Server
