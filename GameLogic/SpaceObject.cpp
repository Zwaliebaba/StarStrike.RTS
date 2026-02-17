#include "pch.h"
#include "SpaceObject.h"

namespace Neuron
{
  void SpaceObject::Update(float _deltaT)
  {
    switch (state.type)
    {
    case SpaceObjectType::Ship:
      UpdateShip(_deltaT);
      break;
    case SpaceObjectType::Asteroid:
    case SpaceObjectType::Crate:
    case SpaceObjectType::JumpGate:
    case SpaceObjectType::Projectile:
    case SpaceObjectType::Station:
    case SpaceObjectType::Turret:
    default:
      UpdateStatic(_deltaT);
      break;
    }
  }

  float SpaceObject::GetMaxSpeed() const noexcept
  {
    if (state.type == SpaceObjectType::Ship)
    {
      auto sc = static_cast<ShipClass>(state.subclass);
      if (static_cast<uint8_t>(sc) < static_cast<uint8_t>(ShipClass::Count))
        return GetShipDef(sc).maxSpeed;
    }
    return 0.0f;
  }

  float SpaceObject::GetAcceleration() const noexcept
  {
    if (state.type == SpaceObjectType::Ship)
    {
      auto sc = static_cast<ShipClass>(state.subclass);
      if (static_cast<uint8_t>(sc) < static_cast<uint8_t>(ShipClass::Count))
        return GetShipDef(sc).acceleration;
    }
    return 0.0f;
  }

  float SpaceObject::GetCollisionRadius() const noexcept
  {
    if (state.type == SpaceObjectType::Ship)
    {
      auto sc = static_cast<ShipClass>(state.subclass);
      if (static_cast<uint8_t>(sc) < static_cast<uint8_t>(ShipClass::Count))
        return GetShipDef(sc).collisionRadius;
    }

    // Default radii for non-ship types
    switch (state.type)
    {
    case SpaceObjectType::Asteroid:  return 10.0f;
    case SpaceObjectType::Station:   return 20.0f;
    case SpaceObjectType::JumpGate:  return 15.0f;
    case SpaceObjectType::Crate:     return 3.0f;
    case SpaceObjectType::Turret:    return 5.0f;
    default:                         return 0.0f;
    }
  }

  void SpaceObject::UpdateShip(float _deltaT)
  {
    if (!hasTarget)
    {
      // Decelerate to stop
      XMVECTOR vel = XMLoadFloat3(&state.velocity);
      float speed = Math::Length(vel);
      if (speed > 0.01f)
      {
        float decel = GetAcceleration() * _deltaT;
        float newSpeed = (std::max)(0.0f, speed - decel);
        vel = XMVectorScale(Math::Normalize(vel), newSpeed);
        XMStoreFloat3(&state.velocity, vel);
      }
      else
      {
        state.velocity = {0.f, 0.f, 0.f};
        state.flags &= ~static_cast<uint16_t>(ObjectFlags::Moving);
      }
    }
    else
    {
      XMVECTOR pos = XMLoadFloat3(&state.position);
      XMVECTOR target = XMLoadFloat3(&targetPosition);
      XMVECTOR toTarget = XMVectorSubtract(target, pos);
      float dist = Math::Length(toTarget);

      float maxSpeed = GetMaxSpeed();
      float accel = GetAcceleration();
      float stoppingDist = (maxSpeed * maxSpeed) / (2.0f * accel);

      if (dist < 1.0f)
      {
        hasTarget = false;
        state.velocity = {0.f, 0.f, 0.f};
        state.flags &= ~static_cast<uint16_t>(ObjectFlags::Moving);
      }
      else
      {
        XMVECTOR dir = Math::Normalize(toTarget);

        // Face toward target (yaw)
        state.yaw = atan2f(Math::GetX(dir), Math::GetZ(dir));

        // Accelerate or decelerate
        XMVECTOR vel = XMLoadFloat3(&state.velocity);
        float currentSpeed = Math::Length(vel);

        float desiredSpeed = maxSpeed;
        if (dist < stoppingDist)
          desiredSpeed = maxSpeed * (dist / stoppingDist);

        float newSpeed = currentSpeed + accel * _deltaT;
        if (newSpeed > desiredSpeed)
          newSpeed = desiredSpeed;

        vel = XMVectorScale(dir, newSpeed);
        XMStoreFloat3(&state.velocity, vel);
        state.flags |= static_cast<uint16_t>(ObjectFlags::Moving);
      }
    }

    // Integrate position
    XMVECTOR pos = XMLoadFloat3(&state.position);
    XMVECTOR vel = XMLoadFloat3(&state.velocity);
    pos = XMVectorAdd(pos, XMVectorScale(vel, _deltaT));
    XMStoreFloat3(&state.position, pos);
  }

  void SpaceObject::UpdateStatic([[maybe_unused]] float _deltaT)
  {
    // Static objects do nothing in MVP
  }
}
