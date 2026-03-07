#pragma once

class DescriptorCache;

class RootParameter
{
  friend class RootSignature;

public:
  RootParameter() { m_RootParam.ParameterType = static_cast<D3D12_ROOT_PARAMETER_TYPE>(0xFFFFFFFF); }

  ~RootParameter() { Clear(); }

  void Clear()
  {
    if (m_RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      delete[] m_RootParam.DescriptorTable.pDescriptorRanges;

    m_RootParam.ParameterType = static_cast<D3D12_ROOT_PARAMETER_TYPE>(0xFFFFFFFF);
  }

  void InitAsConstants(UINT Register, UINT NumDwords, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL,
    UINT Space = 0)
  {
    m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    m_RootParam.ShaderVisibility = Visibility;
    m_RootParam.Constants.Num32BitValues = NumDwords;
    m_RootParam.Constants.ShaderRegister = Register;
    m_RootParam.Constants.RegisterSpace = Space;
  }

  void InitAsConstantBuffer(UINT Register, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL, UINT Space = 0)
  {
    m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    m_RootParam.ShaderVisibility = Visibility;
    m_RootParam.Descriptor.ShaderRegister = Register;
    m_RootParam.Descriptor.RegisterSpace = Space;
  }

  void InitAsBufferSRV(UINT Register, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL, UINT Space = 0)
  {
    m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    m_RootParam.ShaderVisibility = Visibility;
    m_RootParam.Descriptor.ShaderRegister = Register;
    m_RootParam.Descriptor.RegisterSpace = Space;
  }

  void InitAsBufferUAV(UINT Register, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL, UINT Space = 0)
  {
    m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    m_RootParam.ShaderVisibility = Visibility;
    m_RootParam.Descriptor.ShaderRegister = Register;
    m_RootParam.Descriptor.RegisterSpace = Space;
  }

  void InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE Type, UINT Register, UINT Count,
    D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL, UINT Space = 0)
  {
    InitAsDescriptorTable(1, Visibility);
    SetTableRange(0, Type, Register, Count, Space);
  }

  void InitAsDescriptorTable(UINT RangeCount, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
  {
    m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    m_RootParam.ShaderVisibility = Visibility;
    m_RootParam.DescriptorTable.NumDescriptorRanges = RangeCount;
    m_RootParam.DescriptorTable.pDescriptorRanges = NEW D3D12_DESCRIPTOR_RANGE[RangeCount];
  }

  void SetTableRange(UINT RangeIndex, D3D12_DESCRIPTOR_RANGE_TYPE Type, UINT Register, UINT Count, UINT Space = 0)
  {
    auto range = const_cast<D3D12_DESCRIPTOR_RANGE*>(m_RootParam.DescriptorTable.pDescriptorRanges + RangeIndex);
    range->RangeType = Type;
    range->NumDescriptors = Count;
    range->BaseShaderRegister = Register;
    range->RegisterSpace = Space;
    range->OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
  }

  const D3D12_ROOT_PARAMETER& operator()(void) const { return m_RootParam; }

protected:
  D3D12_ROOT_PARAMETER m_RootParam;
};

// Maximum 64 DWORDS divied up amongst all root parameters.
// Root constants = 1 DWORD * NumConstants
// Root descriptor (CBV, SRV, or UAV) = 2 DWORDs each
// Descriptor table pointer = 1 DWORD
// Static samplers = 0 DWORDS (compiled into shader)
class RootSignature
{
  friend class DynamicDescriptorHeap;

public:
  RootSignature(UINT NumRootParams = 0, UINT NumStaticSamplers = 0)
    : m_Finalized(false),
    m_NumParameters(NumRootParams) {
    Reset(NumRootParams, NumStaticSamplers);
  }

  ~RootSignature()
  {
    if (m_signature)
    {
      m_signature->Release();
      m_signature = nullptr;
    }
  }

  void Load(const std::wstring_view _name, const void* _shader, size_t _size)
  {
    check_hresult(Graphics::Core::GetD3DDevice()->CreateRootSignature(0, _shader, _size, IID_PPV_ARGS(&m_signature)));
    m_signature->SetName(_name.data());
    m_Finalized = true;
  }

  void Reset(UINT NumRootParams, UINT NumStaticSamplers = 0)
  {
    if (NumRootParams > 0)
      m_ParamArray.reset(NEW RootParameter[NumRootParams]);
    else
      m_ParamArray = nullptr;
    m_NumParameters = NumRootParams;

    if (NumStaticSamplers > 0)
      m_SamplerArray.reset(NEW D3D12_STATIC_SAMPLER_DESC[NumStaticSamplers]);
    else
      m_SamplerArray = nullptr;
    m_NumSamplers = NumStaticSamplers;
    m_NumInitializedStaticSamplers = 0;
  }

  RootParameter& operator[](size_t EntryIndex)
  {
    DEBUG_ASSERT(EntryIndex < m_NumParameters);
    return m_ParamArray.get()[EntryIndex];
  }

  const RootParameter& operator[](size_t EntryIndex) const
  {
    DEBUG_ASSERT(EntryIndex < m_NumParameters);
    return m_ParamArray.get()[EntryIndex];
  }

  void InitStaticSampler(UINT Register, const D3D12_SAMPLER_DESC& NonStaticSamplerDesc,
    D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL);

  void Finalize(std::wstring _name, D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE);

  [[nodiscard]] ID3D12RootSignature* GetSignature() const
  {
    if (m_Finalized)
      return m_signature;

    while (!m_Finalized)
      std::this_thread::yield();
    return m_signature;
  }

protected:
  BOOL m_Finalized;
  UINT m_NumParameters{};
  UINT m_NumSamplers{};
  UINT m_NumInitializedStaticSamplers{};
  uint32_t m_DescriptorTableBitMap{};		// One bit is set for root parameters that are non-sampler descriptor tables
  uint32_t m_SamplerTableBitMap{};			// One bit is set for root parameters that are sampler descriptor tables
  uint32_t m_DescriptorTableSize[16]{};		// Non-sampler descriptor tables need to know their descriptor count
  std::unique_ptr<RootParameter[]> m_ParamArray{};
  std::unique_ptr<D3D12_STATIC_SAMPLER_DESC[]> m_SamplerArray{};
  ID3D12RootSignature* m_signature{};
};
