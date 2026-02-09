#pragma once

namespace Neuron::Graphics
{
  enum DDS_ALPHA_MODE : uint32_t
  {
    DDS_ALPHA_MODE_UNKNOWN = 0,
    DDS_ALPHA_MODE_STRAIGHT = 1,
    DDS_ALPHA_MODE_PREMULTIPLIED = 2,
    DDS_ALPHA_MODE_OPAQUE = 3,
    DDS_ALPHA_MODE_CUSTOM = 4,
  };

  // Standard version
  HRESULT LoadDDSTextureFromMemory(_In_ ID3D12Device *d3dDevice, _In_reads_bytes_(ddsDataSize) const uint8_t *ddsData, size_t ddsDataSize, _Outptr_ ID3D12Resource **texture, std::vector<D3D12_SUBRESOURCE_DATA> &subresources, size_t maxsize = 0, _Out_opt_ DDS_ALPHA_MODE *alphaMode = nullptr, _Out_opt_ bool *isCubeMap = nullptr);

  HRESULT LoadDDSTextureFromFile(_In_ ID3D12Device *d3dDevice, _In_z_ const wchar_t *szFileName, _Outptr_ ID3D12Resource **texture, std::unique_ptr<uint8_t[]> &ddsData, std::vector<D3D12_SUBRESOURCE_DATA> &subresources, size_t maxsize = 0, _Out_opt_ DDS_ALPHA_MODE *alphaMode = nullptr, _Out_opt_ bool *isCubeMap = nullptr);
}