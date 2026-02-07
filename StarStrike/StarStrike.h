#pragma once

#include "GameMain.h"

class StarStrike : public GameMain
{
  void Startup() override;
  void Shutdown() override;

  void Update(float _deltaT) override;

  void RenderScene() override;

  void RenderCanvas() override;
};
