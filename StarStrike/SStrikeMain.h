#pragma once

#include "GameMain.h"

class SStrikeMain : public Neuron::GameMain
{
public:
  void Startup() override;
  void Shutdown() override;
  void Update(float _deltaT) override;
  void Render() override;

  void CreateDeviceDependentResources() override;
  void CreateWindowSizeDependentResources() override;
  void ReleaseDeviceDependentResources() override;
  void ReleaseWindowSizeDependentResources() override;
};