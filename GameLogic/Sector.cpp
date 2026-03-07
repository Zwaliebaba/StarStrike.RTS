#include "pch.h"
#include "Sector.h"

namespace Neuron::GameLogic
{

// ── Sector ──────────────────────────────────────────────────────────────────

Sector::Sector(int32_t gridX, int32_t gridY)
    : m_gridX(gridX), m_gridY(gridY)
{
}

Vec3i Sector::minBound() const noexcept
{
    return { m_gridX * SECTOR_SIZE_X,
             m_gridY * SECTOR_SIZE_Y,
             0 };
}

Vec3i Sector::maxBound() const noexcept
{
    return { m_gridX * SECTOR_SIZE_X + SECTOR_SIZE_X,
             m_gridY * SECTOR_SIZE_Y + SECTOR_SIZE_Y,
             SECTOR_SIZE_Z };
}

bool Sector::isInBounds(const Vec3i& worldPos) const noexcept
{
    Vec3i lo = minBound();
    Vec3i hi = maxBound();
    return worldPos.x >= lo.x && worldPos.x < hi.x
        && worldPos.y >= lo.y && worldPos.y < hi.y
        && worldPos.z >= lo.z && worldPos.z < hi.z;
}

ChunkID Sector::worldPosToChunkID(const Vec3i& worldPos) const
{
    Vec3i lo = minBound();
    auto cx = static_cast<uint8_t>((worldPos.x - lo.x) / CHUNK_SIZE);
    auto cy = static_cast<uint8_t>((worldPos.y - lo.y) / CHUNK_SIZE);
    auto cz = static_cast<uint8_t>((worldPos.z - lo.z) / CHUNK_SIZE);
    return makeChunkID(static_cast<uint8_t>(m_gridX),
                       static_cast<uint8_t>(m_gridY),
                       cx, cy, cz);
}

// ── SectorManager ───────────────────────────────────────────────────────────

void SectorManager::init(int32_t gridWidth, int32_t gridHeight)
{
    m_gridWidth  = gridWidth;
    m_gridHeight = gridHeight;
    m_sectors.clear();
    m_sectors.reserve(static_cast<size_t>(gridWidth * gridHeight));

    for (int32_t y = 0; y < gridHeight; ++y)
    {
        for (int32_t x = 0; x < gridWidth; ++x)
        {
            m_sectors.emplace_back(x, y);
        }
    }
}

Sector& SectorManager::getSector(int32_t gridX, int32_t gridY)
{
    ASSERT(gridX >= 0 && gridX < m_gridWidth);
    ASSERT(gridY >= 0 && gridY < m_gridHeight);
    return m_sectors[static_cast<size_t>(gridY * m_gridWidth + gridX)];
}

const Sector& SectorManager::getSector(int32_t gridX, int32_t gridY) const
{
    ASSERT(gridX >= 0 && gridX < m_gridWidth);
    ASSERT(gridY >= 0 && gridY < m_gridHeight);
    return m_sectors[static_cast<size_t>(gridY * m_gridWidth + gridX)];
}

const Sector* SectorManager::findSectorForWorldPos(const Vec3i& worldPos) const
{
    for (const auto& s : m_sectors)
    {
        if (s.isInBounds(worldPos))
            return &s;
    }
    return nullptr;
}

} // namespace Neuron::GameLogic
