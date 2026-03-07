#pragma once

#include <cstdint>
#include <limits>

namespace Neuron
{

// ── Entity Identifiers ──────────────────────────────────────────────────────

using EntityID = uint32_t;
using PlayerID = uint16_t;

inline constexpr EntityID INVALID_ENTITY = std::numeric_limits<EntityID>::max();
inline constexpr PlayerID INVALID_PLAYER = std::numeric_limits<PlayerID>::max();

// ── Chunk Identifiers ───────────────────────────────────────────────────────
//
// ChunkID encoding: (sector_x << 40) | (sector_y << 24)
//                 | (chunk_x << 16) | (chunk_y << 8) | chunk_z

using ChunkID = uint64_t;

inline constexpr ChunkID INVALID_CHUNK = std::numeric_limits<ChunkID>::max();

[[nodiscard]] constexpr ChunkID makeChunkID(
    uint8_t sectorX, uint8_t sectorY,
    uint8_t chunkX, uint8_t chunkY, uint8_t chunkZ) noexcept
{
    return (static_cast<uint64_t>(sectorX) << 40)
         | (static_cast<uint64_t>(sectorY) << 24)
         | (static_cast<uint64_t>(chunkX)  << 16)
         | (static_cast<uint64_t>(chunkY)  <<  8)
         | static_cast<uint64_t>(chunkZ);
}

// ── Math Primitives ─────────────────────────────────────────────────────────

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    [[nodiscard]] constexpr Vec3 operator+(const Vec3& rhs) const noexcept
    {
        return { x + rhs.x, y + rhs.y, z + rhs.z };
    }

    [[nodiscard]] constexpr Vec3 operator-(const Vec3& rhs) const noexcept
    {
        return { x - rhs.x, y - rhs.y, z - rhs.z };
    }

    [[nodiscard]] constexpr Vec3 operator*(float s) const noexcept
    {
        return { x * s, y * s, z * s };
    }

    constexpr Vec3& operator+=(const Vec3& rhs) noexcept
    {
        x += rhs.x; y += rhs.y; z += rhs.z;
        return *this;
    }
};

struct Vec3i
{
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;

    [[nodiscard]] constexpr bool operator==(const Vec3i& rhs) const noexcept = default;
};

struct Quat
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct AABB
{
    Vec3 min;
    Vec3 max;

    [[nodiscard]] constexpr bool contains(const Vec3& p) const noexcept
    {
        return p.x >= min.x && p.x <= max.x
            && p.y >= min.y && p.y <= max.y
            && p.z >= min.z && p.z <= max.z;
    }

    [[nodiscard]] constexpr bool overlaps(const AABB& other) const noexcept
    {
        return min.x <= other.max.x && max.x >= other.min.x
            && min.y <= other.max.y && max.y >= other.min.y
            && min.z <= other.max.z && max.z >= other.min.z;
    }
};

// ── Enumerations ────────────────────────────────────────────────────────────

enum class EntityType : uint8_t
{
    Ship,
    Asteroid,
    Projectile,
    Resource,
    Effect
};

enum class VoxelType : uint8_t
{
    Empty   = 0x00,

    // Terrain types (0x01–0x7F)
    Rock    = 0x01,
    Ice     = 0x02,
    Crystal = 0x03,
    Metal   = 0x04,
    Ore     = 0x05,

    // Special types (0x80–0xFF)
    Fissure = 0x80,
    Hazard  = 0x81,
    Liquid  = 0x82
};

enum class ActionType : uint8_t
{
    Move   = 1,
    Attack = 2,
    Mine   = 3,
    Stop   = 4
};

enum class ShipType : uint8_t
{
    Fighter,
    Corvette,
    Carrier
};

enum class PacketType : uint32_t
{
    // Client → Server
    CmdInput        = 0x0001,
    CmdChat         = 0x0002,
    CmdAck          = 0x0003,
    CmdRequestChunk = 0x0004,

    // Server → Client
    SnapState       = 0x0101,
    SnapChunk       = 0x0102,
    EventChat       = 0x0103,
    EventDamage     = 0x0104,
    Ping            = 0x0105
};

} // namespace Neuron
