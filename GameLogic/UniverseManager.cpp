#include "pch.h"
#include "UniverseManager.h"

namespace Neuron::GameLogic
{

void UniverseManager::init(const UniverseConfig& cfg)
{
    m_sectorMgr.init(cfg.sectorGridX, cfg.sectorGridY);
    DebugTrace("UniverseManager initialized ({} x {} sectors)\n",
               cfg.sectorGridX, cfg.sectorGridY);
}

void UniverseManager::tick(float dt, uint64_t tickNum)
{
    m_entitySystem.tickUpdate(dt, tickNum);
}

} // namespace Neuron::GameLogic
