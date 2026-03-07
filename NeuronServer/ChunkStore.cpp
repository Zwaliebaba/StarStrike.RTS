#include "pch.h"
#include "ChunkStore.h"

#include <sstream>

namespace Neuron::Server
{

ChunkStore::ChunkStore(Database& db)
    : m_db(db)
{
}

bool ChunkStore::loadChunk(ChunkID id, GameLogic::VoxelChunk& outChunk)
{
    if (!m_db.isConnected())
        return false;

    // Query the chunk by its binary ID
    std::ostringstream sql;
    sql << "SELECT voxel_data, version FROM voxel_chunks WHERE chunk_id = '\\x";
    // Encode ChunkID as 8-byte hex
    auto idBytes = reinterpret_cast<const uint8_t*>(&id);
    for (size_t i = 0; i < sizeof(ChunkID); ++i)
    {
        char hex[4];
        snprintf(hex, sizeof(hex), "%02x", idBytes[i]);
        sql << hex;
    }
    sql << "'";

    auto result = m_db.query(sql.str());
    if (!result.has_value() || result->empty())
        return false;

    // Parse result: voxel_data is a hex-encoded BYTEA
    const auto& row = (*result)[0];
    if (row.size() < 2)
        return false;

    // For now, just set the chunk ID and clear it
    // Full BYTEA→RLE deserialization requires binary mode queries (Phase 6 polish)
    outChunk.chunkId = id;
    outChunk.version = std::stoull(row[1]);
    return true;
}

bool ChunkStore::saveChunk(const GameLogic::VoxelChunk& chunk)
{
    if (!m_db.isConnected())
        return false;

    auto rleData = GameLogic::VoxelSystem::serializeChunk(chunk);

    // Encode RLE data as hex for BYTEA
    std::ostringstream hexData;
    hexData << "\\x";
    for (auto b : rleData)
    {
        char hex[4];
        snprintf(hex, sizeof(hex), "%02x", b);
        hexData << hex;
    }

    // Encode ChunkID as hex
    std::ostringstream hexId;
    hexId << "\\x";
    auto idBytes = reinterpret_cast<const uint8_t*>(&chunk.chunkId);
    for (size_t i = 0; i < sizeof(ChunkID); ++i)
    {
        char hex[4];
        snprintf(hex, sizeof(hex), "%02x", idBytes[i]);
        hexId << hex;
    }

    std::ostringstream sql;
    sql << "INSERT INTO voxel_chunks (chunk_id, voxel_data, version) VALUES ('"
        << hexId.str() << "', '" << hexData.str() << "', " << chunk.version
        << ") ON CONFLICT (chunk_id) DO UPDATE SET voxel_data = '"
        << hexData.str() << "', version = voxel_chunks.version + 1, modified_at = NOW()";

    return m_db.execute(sql.str());
}

void ChunkStore::flushDirtyChunks(GameLogic::VoxelSystem& voxelSys)
{
    for (auto& [id, chunk] : voxelSys.getAllChunks())
    {
        if (chunk.dirty)
        {
            if (saveChunk(chunk))
            {
                chunk.dirty = false;
            }
        }
    }
}

void ChunkStore::appendVoxelEvents(const std::vector<GameLogic::VoxelDelta>& deltas)
{
    m_eventBuffer.insert(m_eventBuffer.end(), deltas.begin(), deltas.end());
}

void ChunkStore::flushVoxelEvents()
{
    if (m_eventBuffer.empty() || !m_db.isConnected())
        return;

    std::ostringstream sql;
    sql << "INSERT INTO voxel_events (world_x, world_y, world_z, "
        << "old_type, new_type, player_id, tick_number) VALUES ";

    for (size_t i = 0; i < m_eventBuffer.size(); ++i)
    {
        const auto& d = m_eventBuffer[i];
        if (i > 0) sql << ", ";
        sql << "(" << d.worldPos.x << ", " << d.worldPos.y << ", " << d.worldPos.z
            << ", " << static_cast<int>(d.oldType) << ", " << static_cast<int>(d.newType)
            << ", " << d.playerId << ", " << d.tickNum << ")";
    }

    m_db.execute(sql.str());
    m_eventBuffer.clear();
}

size_t ChunkStore::loadSectorChunks(uint8_t sectorX, uint8_t sectorY,
                                    GameLogic::VoxelSystem& voxelSys)
{
    if (!m_db.isConnected())
        return 0;

    std::ostringstream sql;
    sql << "SELECT chunk_id, voxel_data, version FROM voxel_chunks WHERE sector_id = '"
        << static_cast<int>(sectorX) << "," << static_cast<int>(sectorY) << "'";

    auto result = m_db.query(sql.str());
    if (!result.has_value())
        return 0;

    size_t count = 0;
    for (const auto& row : *result)
    {
        if (row.size() < 3) continue;

        GameLogic::VoxelChunk chunk;
        // Parse chunk data (simplified for MVP)
        chunk.version = std::stoull(row[2]);
        voxelSys.loadChunk(std::move(chunk));
        ++count;
    }
    return count;
}

bool ChunkStore::ensureSchema()
{
    if (!m_db.isConnected())
        return false;

    const char* schema = R"SQL(
        CREATE TABLE IF NOT EXISTS voxel_chunks (
            chunk_id BYTEA PRIMARY KEY,
            sector_id VARCHAR(10),
            voxel_data BYTEA,
            version INT DEFAULT 1,
            modified_at TIMESTAMP DEFAULT NOW(),
            locked_by_player_id INT DEFAULT NULL,
            lock_expiry_tick BIGINT DEFAULT NULL
        );
        CREATE INDEX IF NOT EXISTS idx_chunks_sector ON voxel_chunks(sector_id);

        CREATE TABLE IF NOT EXISTS voxel_events (
            id BIGSERIAL PRIMARY KEY,
            chunk_id BYTEA,
            world_x INT, world_y INT, world_z INT,
            old_type SMALLINT, new_type SMALLINT,
            player_id INT,
            tick_number BIGINT,
            created_at TIMESTAMP DEFAULT NOW()
        );
        CREATE INDEX IF NOT EXISTS idx_voxel_events_chunk_tick ON voxel_events(chunk_id, tick_number);

        CREATE TABLE IF NOT EXISTS players (
            player_id SERIAL PRIMARY KEY,
            username VARCHAR(64) UNIQUE NOT NULL,
            password_hash VARCHAR(128) NOT NULL,
            last_login TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS ships (
            ship_id SERIAL PRIMARY KEY,
            owner_id INT NOT NULL REFERENCES players(player_id),
            pos_x REAL, pos_y REAL, pos_z REAL,
            hp INT, max_hp INT,
            cargo_json JSONB,
            last_saved_tick BIGINT
        );
        CREATE INDEX IF NOT EXISTS idx_ships_owner ON ships(owner_id);
    )SQL";

    return m_db.execute(schema);
}

} // namespace Neuron::Server
