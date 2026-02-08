#include "pch.h"
#include "SStrikeMain.h"

void SStrikeMain::Startup()
{
  m_canvas = std::make_unique<Canvas>();
  CreateDeviceDependentResources();
}
void SStrikeMain::Shutdown()
{
  ReleaseDeviceDependentResources();
  m_canvas.reset();
}

void SStrikeMain::CreateDeviceDependentResources()
{
  GameMain::CreateDeviceDependentResources();
  m_canvas->CreateDeviceDependentResources();
}

void SStrikeMain::CreateWindowSizeDependentResources() { GameMain::CreateWindowSizeDependentResources(); }

void SStrikeMain::ReleaseDeviceDependentResources()
{
  GameMain::ReleaseDeviceDependentResources();
  m_canvas->ReleaseDeviceDependentResources();
}

void SStrikeMain::ReleaseWindowSizeDependentResources() { GameMain::ReleaseWindowSizeDependentResources(); }

void SStrikeMain::Update(float _deltaT) {}

void SStrikeMain::Render() {}

void SStrikeMain::RenderScene() {}

void SStrikeMain::RenderCanvas()
{
  GameMain::RenderCanvas();
  m_canvas->Render();
}
