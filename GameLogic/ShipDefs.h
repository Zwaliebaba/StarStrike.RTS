#pragma once

#include "WorldTypes.h"

namespace Neuron
{
  struct ShipDefinition
  {
    ShipClass      shipClass;
    float          maxSpeed;
    float          acceleration;
    float          turnRate;
    float          maxHitpoints;
    float          collisionRadius;
    const char*    name;
    const wchar_t* meshFile;
  };

  inline constexpr ShipDefinition SHIP_DEFINITIONS[] =
  {
    { ShipClass::Asteria,   50.0f, 20.0f, 2.0f, 100.0f, 5.0f, "Asteria",   L"Objects\\Hulls\\hull_asteria.cmo" },
    { ShipClass::Aurora,    70.0f, 30.0f, 3.0f,  80.0f, 4.0f, "Aurora",     L"Objects\\Hulls\\hull_aurora.cmo" },
    { ShipClass::Avalanche, 40.0f, 15.0f, 1.5f, 150.0f, 7.0f, "Avalanche", L"Objects\\Hulls\\hull_avalanche.cmo" },
  };

  [[nodiscard]] inline const ShipDefinition& GetShipDef(ShipClass _class) noexcept
  {
    return SHIP_DEFINITIONS[static_cast<size_t>(_class)];
  }
}
