#pragma once

#include "Color.h"

class SamplerDesc : public D3D12_SAMPLER_DESC
{
public:
  // These defaults match the default values for HLSL-defined root
  // signature static samplers.  So not overriding them here means
  // you can safely not define them in HLSL.
  SamplerDesc(D3D12_COMPARISON_FUNC _comparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL)
  {
    Filter = D3D12_FILTER_ANISOTROPIC;
    AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    MipLODBias = 0.0f;
    MaxAnisotropy = 16;
    ComparisonFunc = _comparisonFunc;
    BorderColor[0] = 1.0f;
    BorderColor[1] = 1.0f;
    BorderColor[2] = 1.0f;
    BorderColor[3] = 1.0f;
    MinLOD = 0.0f;
    MaxLOD = D3D12_FLOAT32_MAX;
  }

  void SetTextureAddressMode(D3D12_TEXTURE_ADDRESS_MODE AddressMode)
  {
    AddressU = AddressMode;
    AddressV = AddressMode;
    AddressW = AddressMode;
  }

  void SetBorderColor(color_t Border)
  {
    BorderColor[0] = XMVectorGetX(Border);
    BorderColor[1] = XMVectorGetY(Border);
    BorderColor[2] = XMVectorGetZ(Border);
    BorderColor[3] = XMVectorGetW(Border);
  }

  // Allocate new descriptor as needed; return handle to existing descriptor when possible
  D3D12_CPU_DESCRIPTOR_HANDLE CreateDescriptor() const;

  // Create descriptor in place (no deduplication).  Handle must be preallocated
  void CreateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE Handle) const;
};
