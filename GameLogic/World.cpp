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
  }

  ObjectId World::SpawnObject(WorldObjectType _type, uint8_t _subclass, XMFLOAT3 _position, uint32_t _ownerClientId)
  {
    ObjectId id = m_nextId++;

    WorldObject obj;
    obj.state.id = id;
    obj.state.type = _type;
    obj.state.subclass = _subclass;
    obj.state.position = _position;
    obj.state.flags = static_cast<uint16_t>(ObjectFlags::Active);
    obj.ownerClientId = _ownerClientId;

    if (_type == WorldObjectType::Ship)
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

  WorldObject* World::GetObject(ObjectId _id)
  {
    auto it = m_objects.find(_id);
    return it != m_objects.end() ? &it->second : nullptr;
  }

  const WorldObject* World::GetObject(ObjectId _id) const
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
