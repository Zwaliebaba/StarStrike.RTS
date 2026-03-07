#pragma once

#include "Types.h"
#include "Constants.h"

#include <cstdint>

namespace Neuron
{

// ── Packet Header ───────────────────────────────────────────────────────────
//
// Wire format (little-endian):
//   [Magic:  u32]  = PACKET_MAGIC (0x53535452 "SSTR")
//   [Type:   u8 ]  = PacketType
//   [Flags:  u8 ]  = Reserved (0)
//   [Pad:    u16]  = Reserved (0)
//   [Seq:    u32]  = Sequence number (per-sender monotonic)
//   [Size:   u16]  = Payload byte count (after header, before CRC)
//   [CRC:    u32]  = CRC-32C over payload bytes only
//   [Payload: variable]
//
// Total header size: 18 bytes

inline constexpr uint32_t PACKET_HEADER_SIZE = 18;

struct PacketHeader
{
    uint32_t magic    = PACKET_MAGIC;
    uint8_t  type     = 0;
    uint8_t  flags    = 0;
    uint16_t reserved = 0;
    uint32_t sequence = 0;
    uint16_t payloadSize = 0;
    uint32_t crc      = 0;
};

// ── Client → Server Packets ─────────────────────────────────────────────────

struct CmdInput
{
    static constexpr uint8_t TYPE = static_cast<uint8_t>(PacketType::CmdInput);

    PlayerID   playerId  = INVALID_PLAYER;
    ActionType action    = ActionType::Stop;
    float      targetX   = 0.0f;
    float      targetY   = 0.0f;
    float      targetZ   = 0.0f;
    EntityID   targetEntity = INVALID_ENTITY;
};

struct CmdChat
{
    static constexpr uint8_t TYPE = static_cast<uint8_t>(PacketType::CmdChat);

    PlayerID playerId = INVALID_PLAYER;
    char     message[256]{};
    uint16_t messageLen = 0;
};

struct CmdRequestChunk
{
    static constexpr uint8_t TYPE = static_cast<uint8_t>(PacketType::CmdRequestChunk);

    PlayerID playerId = INVALID_PLAYER;
    ChunkID  chunkId  = INVALID_CHUNK;
};

// ── Server → Client Packets ─────────────────────────────────────────────────

struct SnapEntityData
{
    EntityID   entityId = INVALID_ENTITY;
    EntityType type     = EntityType::Ship;
    Vec3       position;
    Vec3       velocity;
    float      health   = 0.0f;
    PlayerID   ownerId  = INVALID_PLAYER;
};

struct SnapState
{
    static constexpr uint8_t TYPE = static_cast<uint8_t>(PacketType::SnapState);
    static constexpr uint32_t MAX_ENTITIES_PER_SNAP = 64;

    uint64_t       serverTick  = 0;
    uint16_t       entityCount = 0;
    SnapEntityData entities[MAX_ENTITIES_PER_SNAP]{};
};

struct PingPacket
{
    static constexpr uint8_t TYPE = static_cast<uint8_t>(PacketType::Ping);

    uint64_t serverTick  = 0;
    uint64_t serverTimeUs = 0;
};

} // namespace Neuron
