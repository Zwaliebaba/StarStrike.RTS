#include "pch.h"
#include "SimulationEngine.h"

namespace Neuron::Server
{

void SimulationEngine::tick(uint64_t tickNum)
{
    m_tickCount = tickNum;

    phase1_inputProcessing();
    phase2_voxelUpdate();
    phase3_physics();
    phase4_combat();
    phase5_mining();
    phase6_effects();
}

// ── Phase stubs (filled in during later implementation phases) ───────────────

void SimulationEngine::phase1_inputProcessing()
{
    // TODO Phase 3+: process enqueued player commands
}

void SimulationEngine::phase2_voxelUpdate()
{
    // TODO Phase 3: apply voxel modifications
}

void SimulationEngine::phase3_physics()
{
    // TODO Phase 3: collision detection + response
}

void SimulationEngine::phase4_combat()
{
    // TODO Phase 5: weapon fire, damage, destruction
}

void SimulationEngine::phase5_mining()
{
    // TODO Phase 5: resource extraction logic
}

void SimulationEngine::phase6_effects()
{
    // TODO Phase 5: particle / explosion / visual effect bookkeeping
}

} // namespace Neuron::Server
