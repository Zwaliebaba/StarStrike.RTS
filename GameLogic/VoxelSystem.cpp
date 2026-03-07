#include "pch.h"
#include "VoxelSystem.h"

namespace Neuron::GameLogic
{

// ── Coordinate Helpers ──────────────────────────────────────────────────────

ChunkID VoxelSystem::worldPosToChunkID(const Vec3i& worldPos,
                                       uint8_t sectorX, uint8_t sectorY)
{
    auto chunkX = static_cast<uint8_t>(worldPos.x / CHUNK_SIZE);
    auto chunkY = static_cast<uint8_t>(worldPos.y / CHUNK_SIZE);
    auto chunkZ = static_cast<uint8_t>(worldPos.z / CHUNK_SIZE);
    return makeChunkID(sectorX, sectorY, chunkX, chunkY, chunkZ);
}

Vec3i VoxelSystem::worldPosToLocalPos(const Vec3i& worldPos)
{
    // Modulo that handles negative coords via ((x % n) + n) % n
    auto mod = [](int32_t v, int32_t n) -> int32_t {
        return ((v % n) + n) % n;
    };
    return { mod(worldPos.x, CHUNK_SIZE),
             mod(worldPos.y, CHUNK_SIZE),
             mod(worldPos.z, CHUNK_SIZE) };
}

// ── Voxel Accessors ─────────────────────────────────────────────────────────

void VoxelSystem::setVoxel(const Vec3i& worldPos, uint8_t type,
                           PlayerID playerId, uint64_t tickNum)
{
    // For now, iterate chunks to find the matching one by minCorner range.
    // This is O(N) in chunk count — acceptable for MVP; can be optimised later
    // with a spatial index or direct ChunkID computation from world coords.
    for (auto& [id, chunk] : m_chunks)
    {
        Vec3i localPos = {
            worldPos.x - chunk.minCorner.x,
            worldPos.y - chunk.minCorner.y,
            worldPos.z - chunk.minCorner.z
        };
        if (localPos.x >= 0 && localPos.x < CHUNK_SIZE &&
            localPos.y >= 0 && localPos.y < CHUNK_SIZE &&
            localPos.z >= 0 && localPos.z < CHUNK_SIZE)
        {
            uint8_t oldType = chunk.voxels[localPos.x][localPos.y][localPos.z];
            chunk.voxels[localPos.x][localPos.y][localPos.z] = type;
            chunk.dirty        = true;
            chunk.modifiedTick = tickNum;
            ++chunk.version;

            m_deltaBuffer.push_back(VoxelDelta{
                .worldPos = worldPos,
                .oldType  = oldType,
                .newType  = type,
                .playerId = playerId,
                .tickNum  = tickNum
            });
            return;
        }
    }
}

uint8_t VoxelSystem::getVoxel(const Vec3i& worldPos) const
{
    for (const auto& [id, chunk] : m_chunks)
    {
        Vec3i localPos = {
            worldPos.x - chunk.minCorner.x,
            worldPos.y - chunk.minCorner.y,
            worldPos.z - chunk.minCorner.z
        };
        if (localPos.x >= 0 && localPos.x < CHUNK_SIZE &&
            localPos.y >= 0 && localPos.y < CHUNK_SIZE &&
            localPos.z >= 0 && localPos.z < CHUNK_SIZE)
        {
            return chunk.voxels[localPos.x][localPos.y][localPos.z];
        }
    }
    return static_cast<uint8_t>(VoxelType::Empty);
}

void VoxelSystem::loadChunk(VoxelChunk&& chunk)
{
    ChunkID id = chunk.chunkId;
    m_chunks.insert_or_assign(id, std::move(chunk));
}

VoxelChunk* VoxelSystem::getChunk(ChunkID id)
{
    auto it = m_chunks.find(id);
    return (it != m_chunks.end()) ? &it->second : nullptr;
}

const VoxelChunk* VoxelSystem::getChunk(ChunkID id) const
{
    auto it = m_chunks.find(id);
    return (it != m_chunks.end()) ? &it->second : nullptr;
}

std::vector<VoxelDelta> VoxelSystem::consumeDeltas()
{
    std::vector<VoxelDelta> out;
    out.swap(m_deltaBuffer);
    return out;
}

// ── RLE Serialization ───────────────────────────────────────────────────────
//
// Format: sequence of (count: u16, type: u8) pairs.
// Voxels are iterated in x → y → z order (row-major).
// Maximum run length per entry = 65535.

std::vector<uint8_t> VoxelSystem::serializeChunk(const VoxelChunk& chunk)
{
    std::vector<uint8_t> out;
    out.reserve(256); // Typical sparse chunk is very small

    uint8_t  runType  = chunk.voxels[0][0][0];
    uint16_t runCount = 1;

    auto flushRun = [&]() {
        out.push_back(static_cast<uint8_t>(runCount & 0xFF));
        out.push_back(static_cast<uint8_t>((runCount >> 8) & 0xFF));
        out.push_back(runType);
    };

    for (int32_t x = 0; x < CHUNK_SIZE; ++x)
    {
        for (int32_t y = 0; y < CHUNK_SIZE; ++y)
        {
            for (int32_t z = 0; z < CHUNK_SIZE; ++z)
            {
                if (x == 0 && y == 0 && z == 0) continue; // First voxel handled above

                uint8_t v = chunk.voxels[x][y][z];
                if (v == runType && runCount < 65535)
                {
                    ++runCount;
                }
                else
                {
                    flushRun();
                    runType  = v;
                    runCount = 1;
                }
            }
        }
    }
    flushRun(); // Final run
    return out;
}

bool VoxelSystem::deserializeChunk(const std::vector<uint8_t>& data, VoxelChunk& chunk)
{
    size_t pos = 0;
    int32_t voxelIndex = 0;
    const int32_t totalVoxels = CHUNK_VOLUME;

    while (pos + 2 < data.size() && voxelIndex < totalVoxels)
    {
        uint16_t count = static_cast<uint16_t>(data[pos])
                       | (static_cast<uint16_t>(data[pos + 1]) << 8);
        uint8_t  type  = data[pos + 2];
        pos += 3;

        if (count == 0)
            return false; // Malformed: zero-length run

        for (uint16_t i = 0; i < count && voxelIndex < totalVoxels; ++i)
        {
            int32_t x = voxelIndex / (CHUNK_SIZE * CHUNK_SIZE);
            int32_t y = (voxelIndex / CHUNK_SIZE) % CHUNK_SIZE;
            int32_t z = voxelIndex % CHUNK_SIZE;
            chunk.voxels[x][y][z] = type;
            ++voxelIndex;
        }
    }

    return (voxelIndex == totalVoxels);
}

} // namespace Neuron::GameLogic
