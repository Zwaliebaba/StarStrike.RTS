#pragma once

#include "Types.h"
#include "Constants.h"

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace Neuron::GameLogic
{

/// A voxel modification delta (for persistence and network replication).
struct VoxelDelta
{
    Vec3i    worldPos  = {};
    uint8_t  oldType   = 0;
    uint8_t  newType   = 0;
    PlayerID playerId  = INVALID_PLAYER;
    uint64_t tickNum   = 0;
};

/// A 32×32×32 voxel chunk.
struct VoxelChunk
{
    ChunkID  chunkId      = INVALID_CHUNK;
    Vec3i    minCorner    = {};          // World-space min corner
    uint8_t  voxels[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE]{};
    bool     dirty        = false;
    uint64_t version      = 1;
    uint64_t modifiedTick = 0;

    void clear() noexcept
    {
        std::memset(voxels, 0, sizeof(voxels));
        dirty   = false;
        version = 1;
    }
};

/// Manages all voxel chunks in the world with RLE serialization.
class VoxelSystem
{
public:
    VoxelSystem() = default;

    /// Set a single voxel at a world position. Marks the owning chunk dirty.
    void setVoxel(const Vec3i& worldPos, uint8_t type, PlayerID playerId, uint64_t tickNum);

    /// Get the voxel type at a world position (returns VoxelType::Empty if chunk not loaded).
    [[nodiscard]] uint8_t getVoxel(const Vec3i& worldPos) const;

    /// Insert or replace a chunk in the system.
    void loadChunk(VoxelChunk&& chunk);

    /// Get a pointer to a chunk (or nullptr if not loaded).
    [[nodiscard]] VoxelChunk* getChunk(ChunkID id);
    [[nodiscard]] const VoxelChunk* getChunk(ChunkID id) const;

    /// Get all loaded chunks.
    [[nodiscard]] const std::unordered_map<ChunkID, VoxelChunk>& getAllChunks() const noexcept { return m_chunks; }
    [[nodiscard]] std::unordered_map<ChunkID, VoxelChunk>& getAllChunks() noexcept { return m_chunks; }

    /// Consume and clear the delta buffer (used by persistence and network).
    [[nodiscard]] std::vector<VoxelDelta> consumeDeltas();

    /// Number of loaded chunks.
    [[nodiscard]] size_t chunkCount() const noexcept { return m_chunks.size(); }

    // ── RLE Serialization ───────────────────────────────────────────────────

    /// Serialize a chunk's voxel data to RLE bytes.
    [[nodiscard]] static std::vector<uint8_t> serializeChunk(const VoxelChunk& chunk);

    /// Deserialize RLE bytes into a chunk's voxel data.
    /// Returns false if the data is malformed.
    [[nodiscard]] static bool deserializeChunk(const std::vector<uint8_t>& data, VoxelChunk& chunk);

    // ── Coordinate Helpers ──────────────────────────────────────────────────

    /// Convert a world position to the ChunkID that contains it.
    [[nodiscard]] static ChunkID worldPosToChunkID(const Vec3i& worldPos,
                                                   uint8_t sectorX, uint8_t sectorY);

    /// Convert a world position to the local voxel index within its chunk.
    [[nodiscard]] static Vec3i worldPosToLocalPos(const Vec3i& worldPos);

private:
    std::unordered_map<ChunkID, VoxelChunk> m_chunks;
    std::vector<VoxelDelta>                 m_deltaBuffer;
};

} // namespace Neuron::GameLogic
