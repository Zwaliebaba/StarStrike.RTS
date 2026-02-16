#pragma once

#include "WorldTypes.h"
#include "ShipDefs.h"

namespace Neuron
{
  class WorldObject
  {
  public:
    ObjectState state;
    XMFLOAT3    targetPosition = {0.f, 0.f, 0.f};
    bool        hasTarget      = false;
    uint32_t    ownerClientId  = 0;

    void Update(float _deltaT);

    [[nodiscard]] float GetMaxSpeed() const noexcept;
    [[nodiscard]] float GetAcceleration() const noexcept;

  private:
    void UpdateShip(float _deltaT);
    void UpdateStatic(float _deltaT);
  };
}
