#include "pch.h"
#include "World.h"

namespace Neuron
{
  void World::Startup(float _worldSize)
  {
    m_worldSize = _worldSize;
    m_objects.clear();
    m_nextId = 1;
    DebugTrace("World started: size={}\n", m_worldSize);
  }

  void World::Shutdown()
  {
    m_objects.clear();
    DebugTrace("World shutdown\n");
  }

  void World::Update(float _deltaT)
  {
    for (auto& [id, obj] : m_objects)
    {
      obj.Update(_deltaT);

      // Clamp to world bounds
      float half = m_worldSize * 0.5f;
      obj.state.position.x = (std::clamp)(obj.state.position.x, -half, half);
      obj.state.position.z = (std::clamp)(obj.state.position.z, -half, half);
    }

    ResolveCollisions();
  }

  void World::ResolveCollisions()
  {
    // Collect objects with nonzero collision radii
    std::vector<ObjectId> collidable;
    collidable.reserve(m_objects.size());
    for (const auto& [id, obj] : m_objects)
    {
      if (obj.GetCollisionRadius() > 0.0f)
        collidable.push_back(id);
    }

    // Multiple iterations to settle chain collisions
    constexpr int MAX_ITERATIONS = 4;

    for (int iter = 0; iter < MAX_ITERATIONS; ++iter)
    {
      bool anyResolved = false;

      for (size_t i = 0; i < collidable.size(); ++i)
      {
        auto& objA = m_objects[collidable[i]];
        float radiusA = objA.GetCollisionRadius();

        for (size_t j = i + 1; j < collidable.size(); ++j)
        {
          auto& objB = m_objects[collidable[j]];
          float radiusB = objB.GetCollisionRadius();

          float dx = objA.state.position.x - objB.state.position.x;
          float dz = objA.state.position.z - objB.state.position.z;
          float distSq = dx * dx + dz * dz;
          float minDist = radiusA + radiusB;

          if (distSq >= minDist * minDist)
            continue;

          float dist = sqrtf(distSq);
          float overlap = minDist - dist;

          // Separation direction (if objects overlap exactly, nudge along X)
          float nx, nz;
          if (dist > 0.001f)
          {
            float invDist = 1.0f / dist;
            nx = dx * invDist;
            nz = dz * invDist;
          }
          else
          {
            nx = 1.0f;
            nz = 0.0f;
          }

          bool movableA = objA.state.type == SpaceObjectType::Ship;
          bool movableB = objB.state.type == SpaceObjectType::Ship;

          if (movableA && movableB)
          {
            float half = overlap * 0.5f;
            objA.state.position.x += nx * half;
            objA.state.position.z += nz * half;
            objB.state.position.x -= nx * half;
            objB.state.position.z -= nz * half;
            CancelVelocityIntoCollision(objA, nx, nz);
            CancelVelocityIntoCollision(objB, -nx, -nz);
          }
          else if (movableA)
          {
            objA.state.position.x += nx * overlap;
            objA.state.position.z += nz * overlap;
            CancelVelocityIntoCollision(objA, nx, nz);
          }
          else if (movableB)
          {
            objB.state.position.x -= nx * overlap;
            objB.state.position.z -= nz * overlap;
            CancelVelocityIntoCollision(objB, -nx, -nz);
          }

          anyResolved = true;
        }
      }

      if (!anyResolved)
        break;
    }
  }

  void World::CancelVelocityIntoCollision(SpaceObject& _obj, float _nx, float _nz)
  {
    // Project velocity onto the collision normal; if it points into the collision, remove that component
    float velDotN = _obj.state.velocity.x * _nx + _obj.state.velocity.z * _nz;
    if (velDotN >= 0.0f)
      return; // velocity already moving away or parallel

    _obj.state.velocity.x -= velDotN * _nx;
    _obj.state.velocity.z -= velDotN * _nz;

    // If the object was heading into this obstacle, cancel the target so it doesn't keep re-accelerating
    if (_obj.hasTarget)
    {
      float toTargetX = _obj.targetPosition.x - _obj.state.position.x;
      float toTargetZ = _obj.targetPosition.z - _obj.state.position.z;
      float targetDotN = toTargetX * _nx + toTargetZ * _nz;
      if (targetDotN < 0.0f)
      {
        _obj.hasTarget = false;
        _obj.state.velocity = {0.f, 0.f, 0.f};
        _obj.state.flags &= ~static_cast<uint16_t>(ObjectFlags::Moving);
      }
    }
  }

  ObjectId World::SpawnObject(SpaceObjectType _type, uint8_t _subclass, XMFLOAT3 _position, uint32_t _ownerClientId)
  {
    ObjectId id = m_nextId++;

    SpaceObject obj;
    obj.state.id = id;
    obj.state.type = _type;
    obj.state.subclass = _subclass;
    obj.state.position = _position;
    obj.state.flags = static_cast<uint16_t>(ObjectFlags::Active);
    obj.ownerClientId = _ownerClientId;

    if (_type == SpaceObjectType::Ship)
    {
      auto sc = static_cast<ShipClass>(_subclass);
      if (static_cast<uint8_t>(sc) < static_cast<uint8_t>(ShipClass::Count))
        obj.state.hitpoints = GetShipDef(sc).maxHitpoints;
    }

    m_objects.emplace(id, obj);
    DebugTrace("Spawned object id={} type={} subclass={}\n", id, static_cast<int>(_type), static_cast<int>(_subclass));
    return id;
  }

  void World::RemoveObject(ObjectId _id)
  {
    m_objects.erase(_id);
  }

  SpaceObject* World::GetObject(ObjectId _id)
  {
    auto it = m_objects.find(_id);
    return it != m_objects.end() ? &it->second : nullptr;
  }

  const SpaceObject* World::GetObject(ObjectId _id) const
  {
    auto it = m_objects.find(_id);
    return it != m_objects.end() ? &it->second : nullptr;
  }

  void World::SetObjectTarget(ObjectId _id, XMFLOAT3 _target)
  {
    if (auto* obj = GetObject(_id))
    {
      obj->targetPosition = _target;
      obj->hasTarget = true;
    }
  }

  void World::StopObject(ObjectId _id)
  {
    if (auto* obj = GetObject(_id))
    {
      obj->hasTarget = false;
    }
  }
}
