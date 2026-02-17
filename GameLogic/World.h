#pragma once

#include "SpaceObject.h"
#include <unordered_map>

namespace Neuron
{
  class World
  {
  public:
    void Startup(float _worldSize = WORLD_DEFAULT_SIZE);
    void Shutdown();
    void Update(float _deltaT);

    [[nodiscard]] ObjectId SpawnObject(SpaceObjectType _type, uint8_t _subclass, XMFLOAT3 _position, uint32_t _ownerClientId = 0);
    void RemoveObject(ObjectId _id);

    [[nodiscard]] SpaceObject* GetObject(ObjectId _id);
    [[nodiscard]] const SpaceObject* GetObject(ObjectId _id) const;

    [[nodiscard]] const std::unordered_map<ObjectId, SpaceObject>& GetObjects() const noexcept { return m_objects; }

    void SetObjectTarget(ObjectId _id, XMFLOAT3 _target);
    void StopObject(ObjectId _id);

    [[nodiscard]] float GetWorldSize() const noexcept { return m_worldSize; }

  private:
    std::unordered_map<ObjectId, SpaceObject> m_objects;
    ObjectId m_nextId = 1;
    float m_worldSize = WORLD_DEFAULT_SIZE;
  };
}
