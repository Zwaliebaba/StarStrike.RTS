#include "pch.h"
#include "SStrikeMain.h"
#include "Rendering/DX12Renderer.h"

using namespace StarStrike;

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

void SStrikeMain::Render()
{
  RenderScene();
  RenderCanvas();

  // Composite the canvas texture over the backbuffer
  if (m_canvas && m_canvas->IsValid())
  {
    DX12Renderer::DrawFullscreenTexture(m_canvas->GetSRV());
  }
}

void SStrikeMain::RenderScene() {}

void SStrikeMain::RenderCanvas()
{
  m_canvas->Render();
}
