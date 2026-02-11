#include "pch.h"
#include "SStrikeMain.h"

#include "Color.h"
#include "Canvas.h"
#include "Rendering/DX12Renderer.h"

using namespace Graphics;
using namespace StarStrike;

void SStrikeMain::Startup()
{
  DX12Renderer::Startup();
  CreateDeviceDependentResources();
  // EditorFont is 256x224 with 16x16 pixel glyphs, 16 chars per row, starting at ASCII 32 (space)
  m_editorFont = Canvas::LoadFont(L"Fonts\\EditorFont-ENG.dds", 16, 16, 16, 32);
}

void SStrikeMain::Shutdown()
{
  ReleaseDeviceDependentResources();
  DX12Renderer::Shutdown();
}

void SStrikeMain::CreateDeviceDependentResources() { GameMain::CreateDeviceDependentResources(); }

void SStrikeMain::CreateWindowSizeDependentResources()
{
  GameMain::CreateWindowSizeDependentResources();
  
  // Notify Canvas of the new backbuffer size
  RECT outputSize = Core::GetOutputSize();
  uint32_t width = static_cast<uint32_t>(outputSize.right - outputSize.left);
  uint32_t height = static_cast<uint32_t>(outputSize.bottom - outputSize.top);
  Canvas::OnResize(width, height);
}

void SStrikeMain::ReleaseDeviceDependentResources() { GameMain::ReleaseDeviceDependentResources(); }

void SStrikeMain::ReleaseWindowSizeDependentResources() { GameMain::ReleaseWindowSizeDependentResources(); }

void SStrikeMain::Update(float _deltaT) {}

void SStrikeMain::Render()
{
  RenderScene();
  RenderCanvas();
}

void SStrikeMain::RenderScene()
{
  auto commandlist = Core::GetCommandList();
  // Set the viewport and scissor rectangle.
  const auto viewport = Core::GetScreenViewport();
  const auto scissorRect = Core::GetScissorRect();

  commandlist->RSSetViewports(1, &viewport);
  commandlist->RSSetScissorRects(1, &scissorRect);

  // Indicate that the back buffer will be used as a render target.
  auto renderTargetView = Core::GetRenderTargetView();
  auto depthStencilView = Core::GetDepthStencilView();
  commandlist->ClearRenderTargetView(renderTargetView, Color::BLACK, 0, nullptr);
  commandlist->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  commandlist->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);

  // TODO: Render 3D scene here
}

void SStrikeMain::RenderCanvas()
{
  using Canvas = Canvas;

  // Ensure we're rendering to the backbuffer
  auto commandlist = Core::GetCommandList();
  auto renderTargetView = Core::GetRenderTargetView();
  commandlist->OMSetRenderTargets(1, &renderTargetView, FALSE, nullptr);

  Canvas::BeginFrame();

  // Example: Draw some test primitives at 1920x1080 logical resolution
  //Canvas::DrawRectangle(100.0f, 100.0f, 500.0f, 300.0f, Color::BLUE);
  //Canvas::DrawRectangleOutline(100.0f, 100.0f, 500.0f, 300.0f, 3.0f, Color::WHITE);
  //Canvas::DrawLine(0.0f, 0.0f, 800.0f, 600.0f, Color::RED);
  //Canvas::DrawCircle(960.0f, 540.0f, 100.0f, Color::GREEN);

  Canvas::DrawText(m_editorFont, 500.0f, 500.0f, "Test", Color::RED, 10.0f);

  Canvas::Render();
}