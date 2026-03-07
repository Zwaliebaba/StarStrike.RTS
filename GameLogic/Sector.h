#pragma once

#include "Types.h"
#include "Constants.h"

#include <cstdint>
#include <vector>

namespace Neuron::GameLogic
{

/// Represents a single sector in the 4×4 universe grid.
/// Each sector spans SECTOR_SIZE_X × SECTOR_SIZE_Y × SECTOR_SIZE_Z voxels.
class Sector
{
public:
    Sector() = default;
    Sector(int32_t gridX, int32_t gridY);

    [[nodiscard]] int32_t gridX() const noexcept { return m_gridX; }
    [[nodiscard]] int32_t gridY() const noexcept { return m_gridY; }

    /// Universe-space min corner of this sector.
    [[nodiscard]] Vec3i minBound() const noexcept;

    /// Universe-space max corner of this sector (exclusive).
    [[nodiscard]] Vec3i maxBound() const noexcept;

    /// Is the given universe position inside this sector?
    [[nodiscard]] bool isInBounds(const Vec3i& universePos) const noexcept;

    /// Convert a universe position inside this sector to its owning ChunkID.
    [[nodiscard]] ChunkID universePosToChunkID(const Vec3i& universePos) const;

private:
    int32_t m_gridX = 0;
    int32_t m_gridY = 0;
};

/// Manages the 4×4 grid of sectors.
class SectorManager
{
public:
    SectorManager() = default;

    /// Initialise the sector grid with the given dimensions.
    void init(int32_t gridWidth = SECTOR_GRID_X, int32_t gridHeight = SECTOR_GRID_Y);

    /// Access a sector by grid coordinates (asserts if out of range).
    [[nodiscard]] Sector& getSector(int32_t gridX, int32_t gridY);
    [[nodiscard]] const Sector& getSector(int32_t gridX, int32_t gridY) const;

    /// Find which sector contains the given universe position (or nullptr).
    [[nodiscard]] const Sector* findSectorForUniversePos(const Vec3i& universePos) const;

    [[nodiscard]] int32_t gridWidth()  const noexcept { return m_gridWidth; }
    [[nodiscard]] int32_t gridHeight() const noexcept { return m_gridHeight; }
    [[nodiscard]] size_t  sectorCount() const noexcept { return m_sectors.size(); }

private:
    std::vector<Sector> m_sectors;
    int32_t m_gridWidth  = 0;
    int32_t m_gridHeight = 0;
};

} // namespace Neuron::GameLogic
