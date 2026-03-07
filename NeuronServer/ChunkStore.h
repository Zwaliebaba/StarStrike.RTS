#pragma once

#include "Database.h"
#include "VoxelSystem.h"

#include <cstdint>
#include <vector>

namespace Neuron::Server
{

/// Persistence layer for voxel chunks.
/// Wraps the Database class and provides load/save/flush operations.
class ChunkStore
{
public:
    explicit ChunkStore(Database& db);

    /// Load a single chunk from the database by ChunkID.
    /// Returns true if the chunk was found and loaded into `outChunk`.
    [[nodiscard]] bool loadChunk(ChunkID id, GameLogic::VoxelChunk& outChunk);

    /// Save a single chunk to the database (INSERT or UPDATE with version check).
    bool saveChunk(const GameLogic::VoxelChunk& chunk);

    /// Flush all dirty chunks from the VoxelSystem to the database.
    /// Called every CHUNK_FLUSH_TICKS (30 sec).
    void flushDirtyChunks(GameLogic::VoxelSystem& voxelSys);

    /// Append voxel deltas to the event buffer.
    void appendVoxelEvents(const std::vector<GameLogic::VoxelDelta>& deltas);

    /// Flush buffered voxel events to the database (batch INSERT).
    /// Called every VOXEL_EVENT_FLUSH_TICKS (1 sec).
    void flushVoxelEvents();

    /// Load all chunks for a given sector from the database into the VoxelSystem.
    /// Returns the number of chunks loaded.
    size_t loadSectorChunks(uint8_t sectorX, uint8_t sectorY,
                            GameLogic::VoxelSystem& voxelSys);

    /// Create the schema tables if they don't exist.
    bool ensureSchema();

private:
    Database&                          m_db;
    std::vector<GameLogic::VoxelDelta> m_eventBuffer;
};

} // namespace Neuron::Server
