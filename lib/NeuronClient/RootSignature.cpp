#include "pch.h"
#include "RootSignature.h"

using namespace Graphics;

void RootSignature::InitStaticSampler(UINT Register, const D3D12_SAMPLER_DESC& NonStaticSamplerDesc,
  D3D12_SHADER_VISIBILITY Visibility)
{
  DEBUG_ASSERT(m_NumInitializedStaticSamplers < m_NumSamplers);
  D3D12_STATIC_SAMPLER_DESC& StaticSamplerDesc = m_SamplerArray[m_NumInitializedStaticSamplers++];

  StaticSamplerDesc.Filter = NonStaticSamplerDesc.Filter;
  StaticSamplerDesc.AddressU = NonStaticSamplerDesc.AddressU;
  StaticSamplerDesc.AddressV = NonStaticSamplerDesc.AddressV;
  StaticSamplerDesc.AddressW = NonStaticSamplerDesc.AddressW;
  StaticSamplerDesc.MipLODBias = NonStaticSamplerDesc.MipLODBias;
  StaticSamplerDesc.MaxAnisotropy = NonStaticSamplerDesc.MaxAnisotropy;
  StaticSamplerDesc.ComparisonFunc = NonStaticSamplerDesc.ComparisonFunc;
  StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
  StaticSamplerDesc.MinLOD = NonStaticSamplerDesc.MinLOD;
  StaticSamplerDesc.MaxLOD = NonStaticSamplerDesc.MaxLOD;
  StaticSamplerDesc.ShaderRegister = Register;
  StaticSamplerDesc.RegisterSpace = 0;
  StaticSamplerDesc.ShaderVisibility = Visibility;

  if (StaticSamplerDesc.AddressU == D3D12_TEXTURE_ADDRESS_MODE_BORDER || StaticSamplerDesc.AddressV ==
    D3D12_TEXTURE_ADDRESS_MODE_BORDER || StaticSamplerDesc.AddressW == D3D12_TEXTURE_ADDRESS_MODE_BORDER)
  {
    if (NonStaticSamplerDesc.BorderColor[3] == 1.0f)
    {
      if (NonStaticSamplerDesc.BorderColor[0] == 1.0f)
        StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
      else
        StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    }
    else
      StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
  }
}

void RootSignature::Finalize(const std::wstring _name, D3D12_ROOT_SIGNATURE_FLAGS Flags)
{
  if (!m_Finalized)
  {
    DEBUG_ASSERT(m_NumInitializedStaticSamplers == m_NumSamplers);

    D3D12_ROOT_SIGNATURE_DESC RootDesc;
    RootDesc.NumParameters = m_NumParameters;
    RootDesc.pParameters = (const D3D12_ROOT_PARAMETER*)m_ParamArray.get();
    RootDesc.NumStaticSamplers = m_NumSamplers;
    RootDesc.pStaticSamplers = static_cast<const D3D12_STATIC_SAMPLER_DESC*>(m_SamplerArray.get());
    RootDesc.Flags = Flags;

    m_DescriptorTableBitMap = 0;
    m_SamplerTableBitMap = 0;

    for (UINT Param = 0; Param < m_NumParameters; ++Param)
    {
      const D3D12_ROOT_PARAMETER& RootParam = RootDesc.pParameters[Param];
      m_DescriptorTableSize[Param] = 0;

      if (RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      {
        DEBUG_ASSERT(RootParam.DescriptorTable.pDescriptorRanges != nullptr);

        // We keep track of sampler descriptor tables separately from CBV_SRV_UAV descriptor tables
        if (RootParam.DescriptorTable.pDescriptorRanges->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
          m_SamplerTableBitMap |= (1 << Param);
        else
          m_DescriptorTableBitMap |= (1 << Param);

        for (UINT TableRange = 0; TableRange < RootParam.DescriptorTable.NumDescriptorRanges; ++TableRange)
          m_DescriptorTableSize[Param] += RootParam.DescriptorTable.pDescriptorRanges[TableRange].NumDescriptors;
      }
    }

    com_ptr<ID3DBlob> pOutBlob, pErrorBlob;

    HRESULT hr = D3D12SerializeRootSignature(&RootDesc, D3D_ROOT_SIGNATURE_VERSION_1, pOutBlob.put(), pErrorBlob.put());
    if (FAILED(hr))
    {
      if (pErrorBlob)
        DebugTrace(static_cast<char*>(pErrorBlob->GetBufferPointer()));

      check_hresult(hr);
    }
    check_hresult(Core::GetD3DDevice()->CreateRootSignature(1, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(),
      IID_PPV_ARGS(&m_signature)));

    check_hresult(m_signature->SetName(_name.c_str()));

    m_Finalized = true;
  }
}
