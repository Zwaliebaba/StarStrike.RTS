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
    sql << "SELECT voxel_data, version FROM voxel_chunks WHERE chunk_id = 0x";
    // Encode ChunkID as 8-byte hex
    auto idBytes = reinterpret_cast<const uint8_t*>(&id);
    for (size_t i = 0; i < sizeof(ChunkID); ++i)
    {
        char hex[4];
        snprintf(hex, sizeof(hex), "%02x", idBytes[i]);
        sql << hex;
    }

    auto result = m_db.query(sql.str());
    if (!result.has_value() || result->empty())
        return false;

    // Parse result
    const auto& row = (*result)[0];
    if (row.size() < 2)
        return false;

    // For now, just set the chunk ID and clear it
    // Full VARBINARY→RLE deserialization requires binary mode queries (Phase 6 polish)
    outChunk.chunkId = id;
    outChunk.version = std::stoull(row[1]);
    return true;
}

bool ChunkStore::saveChunk(const GameLogic::VoxelChunk& chunk)
{
    if (!m_db.isConnected())
        return false;

    auto rleData = GameLogic::VoxelSystem::serializeChunk(chunk);

    // Encode RLE data as hex for VARBINARY
    std::ostringstream hexData;
    hexData << "0x";
    for (auto b : rleData)
    {
        char hex[4];
        snprintf(hex, sizeof(hex), "%02x", b);
        hexData << hex;
    }

    // Encode ChunkID as hex
    std::ostringstream hexId;
    hexId << "0x";
    auto idBytes = reinterpret_cast<const uint8_t*>(&chunk.chunkId);
    for (size_t i = 0; i < sizeof(ChunkID); ++i)
    {
        char hex[4];
        snprintf(hex, sizeof(hex), "%02x", idBytes[i]);
        hexId << hex;
    }

    // T-SQL MERGE for upsert
    std::ostringstream sql;
    sql << "MERGE voxel_chunks AS tgt "
        << "USING (SELECT " << hexId.str() << " AS chunk_id) AS src "
        << "ON tgt.chunk_id = src.chunk_id "
        << "WHEN MATCHED THEN UPDATE SET "
        << "voxel_data = " << hexData.str()
        << ", version = tgt.version + 1"
        << ", modified_at = GETUTCDATE() "
        << "WHEN NOT MATCHED THEN INSERT (chunk_id, voxel_data, version) VALUES ("
        << hexId.str() << ", " << hexData.str() << ", " << chunk.version << ");";

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

    // Create tables individually — T-SQL does not support multi-statement batches
    // with IF NOT EXISTS on CREATE TABLE in all contexts; use separate calls.

    bool ok = true;

    ok = ok && m_db.execute(R"SQL(
        IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'voxel_chunks')
        CREATE TABLE voxel_chunks (
            chunk_id            VARBINARY(8) NOT NULL PRIMARY KEY,
            sector_id           VARCHAR(10),
            voxel_data          VARBINARY(MAX),
            version             INT DEFAULT 1,
            modified_at         DATETIME2 DEFAULT GETUTCDATE(),
            locked_by_player_id INT DEFAULT NULL,
            lock_expiry_tick    BIGINT DEFAULT NULL
        )
    )SQL");

    ok = ok && m_db.execute(R"SQL(
        IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_chunks_sector')
        CREATE INDEX idx_chunks_sector ON voxel_chunks(sector_id)
    )SQL");

    ok = ok && m_db.execute(R"SQL(
        IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'voxel_events')
        CREATE TABLE voxel_events (
            id            BIGINT IDENTITY(1,1) PRIMARY KEY,
            chunk_id      VARBINARY(8) NOT NULL,
            world_x       INT,
            world_y       INT,
            world_z       INT,
            old_type      SMALLINT,
            new_type      SMALLINT,
            player_id     INT,
            tick_number   BIGINT,
            created_at    DATETIME2 DEFAULT GETUTCDATE()
        )
    )SQL");

    ok = ok && m_db.execute(R"SQL(
        IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_voxel_events_chunk_tick')
        CREATE INDEX idx_voxel_events_chunk_tick ON voxel_events(chunk_id, tick_number)
    )SQL");

    ok = ok && m_db.execute(R"SQL(
        IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'players')
        CREATE TABLE players (
            player_id     INT IDENTITY(1,1) PRIMARY KEY,
            username      VARCHAR(64) NOT NULL UNIQUE,
            password_hash VARCHAR(128) NOT NULL,
            last_login    DATETIME2
        )
    )SQL");

    ok = ok && m_db.execute(R"SQL(
        IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'ships')
        CREATE TABLE ships (
            ship_id         INT IDENTITY(1,1) PRIMARY KEY,
            owner_id        INT NOT NULL REFERENCES players(player_id),
            pos_x           REAL,
            pos_y           REAL,
            pos_z           REAL,
            hp              INT,
            max_hp          INT,
            cargo_json      NVARCHAR(MAX),
            last_saved_tick BIGINT
        )
    )SQL");

    ok = ok && m_db.execute(R"SQL(
        IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_ships_owner')
        CREATE INDEX idx_ships_owner ON ships(owner_id)
    )SQL");

    return ok;
}

} // namespace Neuron::Server
