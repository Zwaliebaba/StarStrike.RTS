#pragma once

#include "Canvas.h"
#include "GameMain.h"

class SStrikeMain : public GameMain
{
public:
  void Startup() override;
  void Shutdown() override;

  void CreateDeviceDependentResources() override;
  void CreateWindowSizeDependentResources() override;
  void ReleaseDeviceDependentResources() override;
  void ReleaseWindowSizeDependentResources() override;

  void Update(float _deltaT) override;
  void Render() override;

protected:
  void RenderScene();
  void RenderCanvas();

  std::unique_ptr<Canvas> m_canvas;
};