#pragma once

#include <cstdint>
#include <DirectXMath.h>

namespace Neuron
{
  using ObjectId = uint32_t;
  constexpr ObjectId INVALID_OBJECT_ID = 0;

  enum class WorldObjectType : uint8_t
  {
    Ship,
    Asteroid,
    Crate,
    JumpGate,
    Projectile,
    Station,
    Turret,
    Count
  };

  enum class ShipClass : uint8_t
  {
    Asteria,
    Aurora,
    Avalanche,
    Count
  };

  enum class ObjectFlags : uint16_t
  {
    None   = 0x0000,
    Active = 0x0001,
    Moving = 0x0002,
  };

  struct ObjectState
  {
    ObjectId            id        = INVALID_OBJECT_ID;
    WorldObjectType     type      = WorldObjectType::Ship;
    uint8_t             subclass  = 0;
    uint16_t            flags     = 0;
    DirectX::XMFLOAT3   position  = {0.f, 0.f, 0.f};
    DirectX::XMFLOAT3   velocity  = {0.f, 0.f, 0.f};
    float               yaw       = 0.f;
    float               hitpoints = 100.f;
  };

  constexpr float WORLD_DEFAULT_SIZE = 2000.0f;
}
