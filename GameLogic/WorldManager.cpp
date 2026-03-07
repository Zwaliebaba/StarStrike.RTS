#include "pch.h"
#include "WorldManager.h"

namespace Neuron::GameLogic
{

void WorldManager::init(const WorldConfig& cfg)
{
    m_sectorMgr.init(cfg.sectorGridX, cfg.sectorGridY);
    DebugTrace("WorldManager initialized ({} x {} sectors)\n",
               cfg.sectorGridX, cfg.sectorGridY);
}

void WorldManager::tick(float dt, uint64_t tickNum)
{
    m_entitySystem.tickUpdate(dt, tickNum);
}

} // namespace Neuron::GameLogic
