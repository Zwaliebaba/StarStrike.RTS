-- StarStrike.RTS MS SQL Server Schema
-- Phase 3: Entity System & Voxel Storage
-- Run with: sqlcmd -S localhost -d starstrike -i config\schema.sql

-- ── Voxel Chunks ────────────────────────────────────────────────────────────

IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'voxel_chunks')
CREATE TABLE voxel_chunks (
    chunk_id            VARBINARY(8) NOT NULL PRIMARY KEY,
    sector_id           VARCHAR(10),
    voxel_data          VARBINARY(MAX),
    version             INT DEFAULT 1,
    modified_at         DATETIME2 DEFAULT GETUTCDATE(),
    locked_by_player_id INT DEFAULT NULL,
    lock_expiry_tick    BIGINT DEFAULT NULL
);
GO

IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_chunks_sector')
CREATE INDEX idx_chunks_sector ON voxel_chunks(sector_id);
GO

-- ── Voxel Events (append-only log for crash recovery) ──────────────────────

IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'voxel_events')
CREATE TABLE voxel_events (
    id            BIGINT IDENTITY(1,1) PRIMARY KEY,
    chunk_id      VARBINARY(8) NOT NULL,
    universe_x    INT,
    universe_y    INT,
    universe_z    INT,
    old_type      SMALLINT,
    new_type      SMALLINT,
    player_id     INT,
    tick_number   BIGINT,
    created_at    DATETIME2 DEFAULT GETUTCDATE()
);
GO

IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_voxel_events_chunk_tick')
CREATE INDEX idx_voxel_events_chunk_tick ON voxel_events(chunk_id, tick_number);
GO

-- ── Players ─────────────────────────────────────────────────────────────────

IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'players')
CREATE TABLE players (
    player_id     INT IDENTITY(1,1) PRIMARY KEY,
    username      VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(128) NOT NULL,
    last_login    DATETIME2
);
GO

-- ── Ships ───────────────────────────────────────────────────────────────────

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
);
GO

IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_ships_owner')
CREATE INDEX idx_ships_owner ON ships(owner_id);
GO

-- ── Partitioning Note ───────────────────────────────────────────────────────
-- Consider partition functions and schemes on voxel_chunks if table exceeds
-- 100K rows. Partition by sector_id using a partition function with 4 ranges
-- (one per sector row) would be appropriate.
--
-- ── Transaction Template for Concurrent Chunk Edits ─────────────────────────
-- BEGIN TRANSACTION;
--   UPDATE voxel_chunks SET voxel_data = @data, version = version + 1,
--          locked_by_player_id = NULL
--    WHERE chunk_id = @chunkId AND locked_by_player_id = @playerId;
--   INSERT INTO voxel_events (chunk_id, universe_x, universe_y, universe_z,
--          old_type, new_type, player_id, tick_number)
--    VALUES (@chunkId, @x, @y, @z, @oldType, @newType, @playerId, @tick);
-- COMMIT;
--
-- Default isolation: READ COMMITTED.
-- Use SERIALIZABLE for chunk version-check updates to prevent lost writes.
