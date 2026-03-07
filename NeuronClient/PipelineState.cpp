#include "pch.h"
#include "PipelineState.h"

using namespace Graphics;

GraphicsPSO::GraphicsPSO(const wchar_t* _name)
  : PSO(_name)
{
  ZeroMemory(&m_PSODesc, sizeof(m_PSODesc));
  m_PSODesc.NodeMask = 1;
  m_PSODesc.SampleMask = UINT_MAX;
  m_PSODesc.SampleDesc = DefaultSampleDesc();
  m_PSODesc.InputLayout.NumElements = 0;
}

void GraphicsPSO::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc) { m_PSODesc.DepthStencilState = DepthStencilDesc; }

void GraphicsPSO::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType)
{
  DEBUG_ASSERT_TEXT(TopologyType != D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED, "Can't draw with undefined topology");
  m_PSODesc.PrimitiveTopologyType = TopologyType;
}

void GraphicsPSO::SetDepthTargetFormat(DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
  SetRenderTargetFormats(0, nullptr, DSVFormat, MsaaCount, MsaaQuality);
}

void GraphicsPSO::SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
  SetRenderTargetFormats(1, &RTVFormat, DSVFormat, MsaaCount, MsaaQuality);
}

void GraphicsPSO::SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount,
                                         UINT MsaaQuality)
{
  DEBUG_ASSERT_TEXT(NumRTVs == 0 || RTVFormats != nullptr, "Null format array conflicts with non-zero length");
  for (UINT i = 0; i < NumRTVs; ++i)
  {
    DEBUG_ASSERT(RTVFormats[i] != DXGI_FORMAT_UNKNOWN);
    m_PSODesc.RTVFormats[i] = RTVFormats[i];
  }
  for (UINT i = NumRTVs; i < m_PSODesc.NumRenderTargets; ++i)
    m_PSODesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
  m_PSODesc.NumRenderTargets = NumRTVs;
  m_PSODesc.DSVFormat = DSVFormat;
  m_PSODesc.SampleDesc.Count = MsaaCount;
  m_PSODesc.SampleDesc.Quality = MsaaQuality;
}

void GraphicsPSO::SetInputLayout(const D3D12_INPUT_LAYOUT_DESC* pInputElementDescs)
{
  UINT NumElements = pInputElementDescs->NumElements;

  m_PSODesc.InputLayout.NumElements = NumElements;

  if (NumElements > 0)
  {
    auto NewElements = static_cast<D3D12_INPUT_ELEMENT_DESC*>(malloc(sizeof(D3D12_INPUT_ELEMENT_DESC) * NumElements));
    memcpy(NewElements, pInputElementDescs->pInputElementDescs, NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
    m_inputLayouts.reset(static_cast<const D3D12_INPUT_ELEMENT_DESC*>(NewElements));
  }
  else
    m_inputLayouts = nullptr;
}

void GraphicsPSO::Finalize()
{
  auto device = Core::GetD3DDevice();

  // Make sure the root signature is finalized first
  m_PSODesc.pRootSignature = m_RootSignature->GetSignature();
  DEBUG_ASSERT(m_PSODesc.pRootSignature != nullptr);

  m_PSODesc.InputLayout.pInputElementDescs = nullptr;
  m_PSODesc.InputLayout.pInputElementDescs = m_inputLayouts.get();
  DEBUG_ASSERT(m_PSODesc.DepthStencilState.DepthEnable != (m_PSODesc.DSVFormat == DXGI_FORMAT_UNKNOWN));
  check_hresult(device->CreateGraphicsPipelineState(&m_PSODesc, IID_PPV_ARGS(m_PSO.put())));
  m_PSO->SetName(m_Name);
}

void ComputePSO::Finalize()
{
  auto device = Core::GetD3DDevice();

  // Make sure the root signature is finalized first
  m_PSODesc.pRootSignature = m_RootSignature->GetSignature();
  DEBUG_ASSERT(m_PSODesc.pRootSignature != nullptr);
  check_hresult(device->CreateComputePipelineState(&m_PSODesc, IID_PPV_ARGS(m_PSO.put())));
  m_PSO->SetName(m_Name);
}

ComputePSO::ComputePSO(const wchar_t* Name)
  : PSO(Name)
{
  ZeroMemory(&m_PSODesc, sizeof(m_PSODesc));
  m_PSODesc.NodeMask = 1;
}
