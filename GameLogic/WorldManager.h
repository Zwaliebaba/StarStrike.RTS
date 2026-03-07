#pragma once

#include "EntitySystem.h"
#include "VoxelSystem.h"
#include "Sector.h"

namespace Neuron::Server { class ChunkStore; }

namespace Neuron::GameLogic
{

/// Configuration for WorldManager initialisation.
struct WorldConfig
{
    int32_t sectorGridX = SECTOR_GRID_X;
    int32_t sectorGridY = SECTOR_GRID_Y;
};

/// Top-level orchestrator for the game world state.
/// Owns EntitySystem, VoxelSystem, and SectorManager.
/// ChunkStore (persistence) is held externally by the server and passed in.
class WorldManager
{
public:
    WorldManager() = default;

    /// Initialise the world: set up sector grid.
    void init(const WorldConfig& cfg = {});

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
