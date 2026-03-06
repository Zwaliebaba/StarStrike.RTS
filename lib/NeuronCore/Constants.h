#pragma once

#include <cstdint>

namespace Neuron
{

// ── Server ──────────────────────────────────────────────────────────────────

inline constexpr uint32_t TICK_RATE_HZ           = 60;
inline constexpr float    TICK_INTERVAL_SEC      = 1.0f / TICK_RATE_HZ;       // 16.67 ms
inline constexpr uint32_t TICK_INTERVAL_US       = 16'667;                    // microseconds
inline constexpr uint32_t SNAPSHOT_RATE_HZ       = 20;
inline constexpr uint32_t TICKS_PER_SNAPSHOT     = TICK_RATE_HZ / SNAPSHOT_RATE_HZ; // 3
inline constexpr uint16_t DEFAULT_SERVER_PORT    = 7777;
inline constexpr uint32_t MAX_PLAYERS            = 50;
inline constexpr uint32_t MAX_SHIPS_PER_PLAYER   = 5;
inline constexpr uint32_t MAX_ENTITIES           = 10'000;

// ── Voxel World ─────────────────────────────────────────────────────────────

inline constexpr int32_t  CHUNK_SIZE             = 32;   // voxels per axis
inline constexpr int32_t  CHUNK_VOLUME           = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE; // 32,768
inline constexpr int32_t  SECTOR_GRID_X          = 4;
inline constexpr int32_t  SECTOR_GRID_Y          = 4;
inline constexpr int32_t  SECTOR_COUNT           = SECTOR_GRID_X * SECTOR_GRID_Y; // 16
inline constexpr int32_t  SECTOR_SIZE_X          = 512;  // voxels
inline constexpr int32_t  SECTOR_SIZE_Y          = 512;
inline constexpr int32_t  SECTOR_SIZE_Z          = 256;
inline constexpr int32_t  CHUNKS_PER_SECTOR_X    = SECTOR_SIZE_X / CHUNK_SIZE; // 16
inline constexpr int32_t  CHUNKS_PER_SECTOR_Y    = SECTOR_SIZE_Y / CHUNK_SIZE; // 16
inline constexpr int32_t  CHUNKS_PER_SECTOR_Z    = SECTOR_SIZE_Z / CHUNK_SIZE; // 8

// ── Streaming & LOD ─────────────────────────────────────────────────────────

inline constexpr int32_t  STREAMING_RADIUS_CHUNKS = 8;
inline constexpr int32_t  LOD0_DISTANCE_CHUNKS    = 8;   // full detail
inline constexpr int32_t  LOD1_DISTANCE_CHUNKS    = 16;  // half resolution
inline constexpr int32_t  LOD2_DISTANCE_CHUNKS    = 32;  // quarter resolution

// ── Network ─────────────────────────────────────────────────────────────────

inline constexpr uint32_t MAX_RECV_PER_TICK       = 100;  // max datagrams per tick
inline constexpr uint32_t MAX_COMMANDS_PER_SEC    = 100;  // rate limit per client
inline constexpr uint32_t PACKET_MAGIC            = 0x53535452; // "SSTR" (StarStrike)
inline constexpr uint32_t MAX_PACKET_SIZE         = 1400; // bytes (MTU-safe)

// ── Persistence ─────────────────────────────────────────────────────────────

inline constexpr uint32_t VOXEL_EVENT_FLUSH_TICKS  = TICK_RATE_HZ;       // every 1 sec
inline constexpr uint32_t CHUNK_FLUSH_TICKS        = TICK_RATE_HZ * 30;  // every 30 sec

// ── Physics ─────────────────────────────────────────────────────────────────

inline constexpr float    SHIP_AABB_HALF_EXTENT    = 5.0f;  // 10×10×10 unit box
inline constexpr float    PROJECTILE_AABB_EXTENT   = 0.5f;  // 1×1×1 point-like

// ── Rendering ───────────────────────────────────────────────────────────────

inline constexpr uint32_t TARGET_CLIENT_FPS        = 60;
inline constexpr uint32_t MAX_VISIBLE_CHUNKS       = 200;
inline constexpr uint32_t VRAM_BUDGET_MB           = 512;

} // namespace Neuron
