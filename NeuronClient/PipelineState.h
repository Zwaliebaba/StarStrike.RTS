#pragma once


#include "RootSignature.h"
#include <winrt/base.h> // for winrt::com_ptr, or replace with your com_ptr type


/**
 * @brief Base class for Pipeline State Objects (PSO), manages D3D12 pipeline state lifetime (RAII).
 */
class PSO
{
public:
  /**
   * @brief Destructor automatically releases the pipeline state.
   */
  virtual ~PSO() = default;

  /**
   * @brief Construct with a name.
   */
  PSO(const wchar_t* Name)
    : m_Name(Name),
      m_RootSignature(nullptr)
  {
  }

  /**
   * @brief Set the root signature for this PSO.
   */
  void SetRootSignature(const RootSignature& BindMappings) { m_RootSignature = &BindMappings; }

  /**
   * @brief Get the root signature.
   */
  const RootSignature& GetRootSignature() const
  {
    DEBUG_ASSERT(m_RootSignature != nullptr);
    return *m_RootSignature;
  }

  /**
   * @brief Get the underlying D3D12 pipeline state object.
   */
  [[nodiscard]] virtual ID3D12PipelineState* GetPipelineStateObject()
  {
    return m_PSO.get();
  }

protected:
  const wchar_t* m_Name;
  const RootSignature* m_RootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_PSO;
};

class GraphicsPSO : public PSO
{
public:
  // Start with empty state
  GraphicsPSO(const wchar_t* _name = L"Unnamed Graphics PSO");
  ~GraphicsPSO() override = default;

  void SetBlendState(const D3D12_BLEND_DESC& BlendDesc) { m_PSODesc.BlendState = BlendDesc; }
  void SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc) { m_PSODesc.RasterizerState = RasterizerDesc; }
  void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc);
  void SetSampleMask(UINT SampleMask) { m_PSODesc.SampleMask = SampleMask; }
  void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType);
  void SetDepthTargetFormat(DXGI_FORMAT DSVFormat, UINT MsaaCount = 1, UINT MsaaQuality = 0);
  void SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount = 1, UINT MsaaQuality = 0);
  void SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount = 1,
    UINT MsaaQuality = 0);
  void SetInputLayout(const D3D12_INPUT_LAYOUT_DESC* pInputElementDescs);
  void SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBProps) { m_PSODesc.IBStripCutValue = IBProps; }

  // These const_casts shouldn't be necessary, but we need to fix the API to accept "const void* pShaderBytecode"
  void SetVertexShader(const void* Binary, size_t Size) { m_PSODesc.VS = CD3DX12_SHADER_BYTECODE(Binary, Size); }
  void SetPixelShader(const void* Binary, size_t Size) { m_PSODesc.PS = CD3DX12_SHADER_BYTECODE(Binary, Size); }
  void SetGeometryShader(const void* Binary, size_t Size) { m_PSODesc.GS = CD3DX12_SHADER_BYTECODE(Binary, Size); }
  void SetHullShader(const void* Binary, size_t Size) { m_PSODesc.HS = CD3DX12_SHADER_BYTECODE(Binary, Size); }
  void SetDomainShader(const void* Binary, size_t Size) { m_PSODesc.DS = CD3DX12_SHADER_BYTECODE(Binary, Size); }

  void SetVertexShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSODesc.VS = Binary; }
  void SetPixelShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSODesc.PS = Binary; }
  void SetGeometryShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSODesc.GS = Binary; }
  void SetHullShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSODesc.HS = Binary; }
  void SetDomainShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSODesc.DS = Binary; }

  // Perform validation and compute a hash value for fast state block comparisons
  void Finalize();

private:
  D3D12_GRAPHICS_PIPELINE_STATE_DESC m_PSODesc{};
  std::shared_ptr<const D3D12_INPUT_ELEMENT_DESC> m_inputLayouts;
};

class ComputePSO : public PSO
{
public:
  ComputePSO(const wchar_t* Name = L"Unnamed Compute PSO");

  void SetComputeShader(const void* Binary, size_t Size) { m_PSODesc.CS = CD3DX12_SHADER_BYTECODE(Binary, Size); }
  void SetComputeShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSODesc.CS = Binary; }

  void Finalize();

private:
  D3D12_COMPUTE_PIPELINE_STATE_DESC m_PSODesc{};
};
