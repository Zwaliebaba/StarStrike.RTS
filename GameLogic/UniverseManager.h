#pragma once

#include "EntitySystem.h"
#include "VoxelSystem.h"
#include "Sector.h"

namespace Neuron::Server { class ChunkStore; }

namespace Neuron::GameLogic
{

/// Configuration for UniverseManager initialisation.
struct UniverseConfig
{
    int32_t sectorGridX = SECTOR_GRID_X;
    int32_t sectorGridY = SECTOR_GRID_Y;
};

/// Top-level orchestrator for the game universe state.
/// Owns EntitySystem, VoxelSystem, and SectorManager.
/// ChunkStore (persistence) is held externally by the server and passed in.
class UniverseManager
{
public:
    UniverseManager() = default;

    /// Initialise the universe: set up sector grid.
    void init(const UniverseConfig& cfg = {});

    /// Per-tick update (called by SimulationEngine).
    void tick(float dt, uint64_t tickNum);

    [[nodiscard]] EntitySystem&   getEntitySystem()   noexcept { return m_entitySystem; }
    [[nodiscard]] VoxelSystem&    getVoxelSystem()    noexcept { return m_voxelSystem; }
    [[nodiscard]] SectorManager&  getSectorManager()  noexcept { return m_sectorMgr; }

    [[nodiscard]] const EntitySystem&  getEntitySystem()  const noexcept { return m_entitySystem; }
    [[nodiscard]] const VoxelSystem&   getVoxelSystem()   const noexcept { return m_voxelSystem; }
    [[nodiscard]] const SectorManager& getSectorManager() const noexcept { return m_sectorMgr; }

private:
    EntitySystem  m_entitySystem;
    VoxelSystem   m_voxelSystem;
    SectorManager m_sectorMgr;
};

} // namespace Neuron::GameLogic
