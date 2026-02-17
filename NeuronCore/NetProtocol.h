#pragma once

#include "WorldTypes.h"

namespace Neuron::Net
{
  constexpr uint16_t DEFAULT_PORT       = 27015;
  constexpr uint32_t PROTOCOL_ID        = 0x53545231; // "STR1"
  constexpr uint32_t MAX_CLIENTS        = 64;
  constexpr uint32_t MAX_SNAPSHOT_OBJECTS = 32;
  constexpr float    SNAPSHOT_INTERVAL  = 1.0f / 10.0f; // 10 Hz
  constexpr float    TICK_INTERVAL      = 1.0f / 20.0f; // 20 Hz

  enum class PacketType : uint8_t
  {
    Connect,
    ConnectAck,
    Disconnect,
    ClientInput,
    WorldSnapshot,
    Heartbeat,
    Count
  };

  enum class InputType : uint8_t
  {
    None,
    MoveTo,
    Stop,
    Count
  };

#pragma pack(push, 1)

  struct PacketHeader
  {
    uint32_t   protocolId = PROTOCOL_ID;
    PacketType type       = PacketType::Heartbeat;
    uint16_t   sequence   = 0;
    uint16_t   ack        = 0;
    uint32_t   ackBits    = 0;
  };

  struct ConnectPacket
  {
    PacketHeader header;
  };

  struct ConnectAckPacket
  {
    PacketHeader header;
    uint32_t     clientId       = 0;
    ObjectId     playerObjectId = INVALID_OBJECT_ID;
  };

  struct DisconnectPacket
  {
    PacketHeader header;
  };

  struct ClientInputPacket
  {
    PacketHeader         header;
    uint32_t             clientId       = 0;
    uint32_t             inputSequence  = 0;
    InputType            inputType      = InputType::None;
    DirectX::XMFLOAT3   targetPosition = {0.f, 0.f, 0.f};
  };

  struct SnapshotObjectData
  {
    ObjectId            objectId   = INVALID_OBJECT_ID;
    SpaceObjectType     objectType = SpaceObjectType::Ship;
    uint8_t             subclass   = 0;
    uint16_t            flags      = 0;
    DirectX::XMFLOAT3   position   = {0.f, 0.f, 0.f};
    DirectX::XMFLOAT3   velocity   = {0.f, 0.f, 0.f};
    float               yaw        = 0.f;
    float               hitpoints  = 100.f;
  };

  struct WorldSnapshotPacket
  {
    PacketHeader       header;
    uint32_t           snapshotId          = 0;
    uint32_t           lastProcessedInput  = 0;
    uint16_t           objectCount         = 0;
    SnapshotObjectData objects[MAX_SNAPSHOT_OBJECTS];

    [[nodiscard]] size_t GetSize() const noexcept
    {
      return offsetof(WorldSnapshotPacket, objects) + sizeof(SnapshotObjectData) * objectCount;
    }
  };

  struct HeartbeatPacket
  {
    PacketHeader header;
    uint32_t     clientId = 0;
  };

#pragma pack(pop)

}
