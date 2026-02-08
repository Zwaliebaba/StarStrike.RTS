#include "pch.h"
#include "Canvas.h"

using namespace Neuron::Graphics;

namespace
{
  constexpr DXGI_FORMAT CANVAS_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
}

void Canvas::CreateDeviceDependentResources()
{
  auto device = Core::GetD3DDevice();
  if (!device)
  {
    DebugTrace("Canvas::CreateDeviceDependentResources - No D3D device available\n");
    return;
  }

  // Create render target texture resource
  D3D12_RESOURCE_DESC texDesc = {};
  texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texDesc.Width = WIDTH;
  texDesc.Height = HEIGHT;
  texDesc.DepthOrArraySize = 1;
  texDesc.MipLevels = 1;
  texDesc.Format = CANVAS_FORMAT;
  texDesc.SampleDesc.Count = 1;
  texDesc.SampleDesc.Quality = 0;
  texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

  D3D12_CLEAR_VALUE clearValue = {};
  clearValue.Format = CANVAS_FORMAT;
  clearValue.Color[0] = 0.0f;
  clearValue.Color[1] = 0.0f;
  clearValue.Color[2] = 0.0f;
  clearValue.Color[3] = 0.0f;

  CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);

  HRESULT hr = device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue, IID_PPV_ARGS(m_renderTarget.Put()));

  if (FAILED(hr))
  {
    DebugTrace("Canvas::CreateDeviceDependentResources - Failed to create render target texture\n");
    return;
  }

  m_renderTarget.SetResourceState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

  // Create RTV (Render Target View)
  D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
  rtvDesc.Format = CANVAS_FORMAT;
  rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
  rtvDesc.Texture2D.MipSlice = 0;
  rtvDesc.Texture2D.PlaneSlice = 0;

  auto rtvHandle = Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  m_rtvHandle = rtvHandle;
  device->CreateRenderTargetView(m_renderTarget.GetResource(), &rtvDesc, m_rtvHandle);

  // Create SRV (Shader Resource View) for sampling the texture
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = CANVAS_FORMAT;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Texture2D.MipLevels = 1;
  srvDesc.Texture2D.MostDetailedMip = 0;

  m_srvHandle = Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  device->CreateShaderResourceView(m_renderTarget.GetResource(), &srvDesc, m_srvHandle);

  DebugTrace("Canvas::CreateDeviceDependentResources - Created {}x{} render target texture\n", WIDTH, HEIGHT);
}

void Canvas::ReleaseDeviceDependentResources()
{
  m_renderTarget.Destroy();
  m_rtvHandle = {};
  m_srvHandle = {};
}

void Canvas::Update(float _deltaT) {}

void Canvas::Render()
{
  if (!IsValid()) return;

  auto cmdList = Core::GetCommandList();
  auto stateTracker = Core::GetGpuResourceStateTracker();

  // Transition to render target state
  stateTracker->TransitionResource(m_renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
  stateTracker->FlushResourceBarriers();

  // Set the render target
  cmdList->OMSetRenderTargets(1, &m_rtvHandle, FALSE, nullptr);

  // Clear the render target (transparent)
  constexpr float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  cmdList->ClearRenderTargetView(m_rtvHandle, clearColor, 0, nullptr);

  // Set viewport and scissor rect for the canvas
  D3D12_VIEWPORT viewport = {};
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;
  viewport.Width = static_cast<float>(WIDTH);
  viewport.Height = static_cast<float>(HEIGHT);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;

  D3D12_RECT scissorRect = {};
  scissorRect.left = 0;
  scissorRect.top = 0;
  scissorRect.right = static_cast<LONG>(WIDTH);
  scissorRect.bottom = static_cast<LONG>(HEIGHT);

  cmdList->RSSetViewports(1, &viewport);
  cmdList->RSSetScissorRects(1, &scissorRect);

  // TODO: Add canvas rendering code here (UI elements, HUD, etc.)

  // Transition back to shader resource state for sampling
  stateTracker->TransitionResource(m_renderTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  stateTracker->FlushResourceBarriers();
}