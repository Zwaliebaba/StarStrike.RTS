#pragma once

#include "WorldTypes.h"

namespace Neuron
{
  struct AsteroidDefinition
  {
    AsteroidClass asteroidClass;
    float         collisionRadius;
    float         rotationSpeed;   // radians per second
    const wchar_t* meshFile;
  };

  inline constexpr AsteroidDefinition ASTEROID_DEFINITIONS[] =
  {
    { AsteroidClass::Asteroid01, 8.0f,  0.15f, L"Objects\\Asteroids\\asteroid_01.cmo" },
    { AsteroidClass::Asteroid02, 10.0f, 0.20f, L"Objects\\Asteroids\\asteroid_02.cmo" },
    { AsteroidClass::Asteroid03, 12.0f, 0.10f, L"Objects\\Asteroids\\asteroid_03.cmo" },
    { AsteroidClass::Asteroid04, 9.0f,  0.25f, L"Objects\\Asteroids\\asteroid_04.cmo" },
    { AsteroidClass::Asteroid05, 11.0f, 0.12f, L"Objects\\Asteroids\\asteroid_05.cmo" },
    { AsteroidClass::Asteroid06, 14.0f, 0.08f, L"Objects\\Asteroids\\asteroid_06.cmo" },
  };

  [[nodiscard]] inline const AsteroidDefinition& GetAsteroidDef(AsteroidClass _class) noexcept
  {
    return ASTEROID_DEFINITIONS[static_cast<size_t>(_class)];
  }
}
