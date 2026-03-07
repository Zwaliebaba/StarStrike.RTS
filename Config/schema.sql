-- StarStrike.RTS PostgreSQL Schema
-- Phase 3: Entity System & Voxel Storage
-- Run with: psql -d starstrike -f config/schema.sql

-- ── Voxel Chunks ────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS voxel_chunks (
    chunk_id            BYTEA PRIMARY KEY,
    sector_id           VARCHAR(10),
    voxel_data          BYTEA,
    version             INT DEFAULT 1,
    modified_at         TIMESTAMP DEFAULT NOW(),
    locked_by_player_id INT DEFAULT NULL,
    lock_expiry_tick    BIGINT DEFAULT NULL
);

CREATE INDEX IF NOT EXISTS idx_chunks_sector ON voxel_chunks(sector_id);

-- ── Voxel Events (append-only log for crash recovery) ──────────────────────

CREATE TABLE IF NOT EXISTS voxel_events (
    id            BIGSERIAL PRIMARY KEY,
    chunk_id      BYTEA NOT NULL,
    world_x       INT,
    world_y       INT,
    world_z       INT,
    old_type      SMALLINT,
    new_type      SMALLINT,
    player_id     INT,
    tick_number   BIGINT,
    created_at    TIMESTAMP DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_voxel_events_chunk_tick ON voxel_events(chunk_id, tick_number);

-- ── Players ─────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS players (
    player_id     SERIAL PRIMARY KEY,
    username      VARCHAR(64) UNIQUE NOT NULL,
    password_hash VARCHAR(128) NOT NULL,
    last_login    TIMESTAMP
);

-- ── Ships ───────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS ships (
    ship_id         SERIAL PRIMARY KEY,
    owner_id        INT NOT NULL REFERENCES players(player_id),
    pos_x           REAL,
    pos_y           REAL,
    pos_z           REAL,
    hp              INT,
    max_hp          INT,
    cargo_json      JSONB,
    last_saved_tick BIGINT
);

CREATE INDEX IF NOT EXISTS idx_ships_owner ON ships(owner_id);

-- ── Partitioning Note ───────────────────────────────────────────────────────
-- Consider PARTITION BY LIST (sector_id) on voxel_chunks if table exceeds
-- 100K rows. 4 partitions (one per sector row) would be appropriate.
--
-- ── Transaction Template for Concurrent Chunk Edits ─────────────────────────
-- BEGIN;
--   UPDATE voxel_chunks SET voxel_data = $1, version = version + 1,
--          locked_by_player_id = NULL
--    WHERE chunk_id = $2 AND locked_by_player_id = $3;
--   INSERT INTO voxel_events (chunk_id, world_x, world_y, world_z,
--          old_type, new_type, player_id, tick_number)
--    VALUES ($2, $4, $5, $6, $7, $8, $9, $10);
-- COMMIT;
--
-- Default isolation: READ COMMITTED.
-- Use SERIALIZABLE for chunk version-check updates to prevent lost writes.
