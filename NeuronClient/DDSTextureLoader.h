#pragma once

namespace Neuron::Graphics
{
  // DDS alpha mode flags
  enum class DdsAlphaMode : uint32_t
  {
    Unknown = 0,
    Straight = 1,
    Premultiplied = 2,
    Opaque = 3,
    Custom = 4,
  };

  // DDS loader flags
  enum class DdsLoaderFlags : uint32_t
  {
    Default = 0,
    ForceSRGB = 0x1,
    IgnoreSRGB = 0x2,
    MipReserve = 0x8,
  };

  inline DdsLoaderFlags operator|(DdsLoaderFlags _a, DdsLoaderFlags _b)
  {
    return static_cast<DdsLoaderFlags>(static_cast<uint32_t>(_a) | static_cast<uint32_t>(_b));
  }

  inline DdsLoaderFlags operator&(DdsLoaderFlags _a, DdsLoaderFlags _b)
  {
    return static_cast<DdsLoaderFlags>(static_cast<uint32_t>(_a) & static_cast<uint32_t>(_b));
  }

  // DDS texture loading utilities
  class DdsTextureLoader
  {
    public:
      // Load DDS texture from memory buffer
      static HRESULT LoadFromMemory(ID3D12Device* _device, const uint8_t* _ddsData, size_t _ddsDataSize, ID3D12Resource** _outTexture,
                                    std::vector<D3D12_SUBRESOURCE_DATA>& _outSubresources, size_t _maxSize = 0,
                                    DdsAlphaMode* _outAlphaMode = nullptr, bool* _outIsCubeMap = nullptr);

      // Load DDS texture from file (uses engine's BinaryFile::ReadFile)
      // _outFileData receives ownership of the file buffer - keep alive until upload completes
      static HRESULT LoadFromFile(ID3D12Device* _device, const std::wstring& _filePath, ID3D12Resource** _outTexture,
                                  std::vector<D3D12_SUBRESOURCE_DATA>& _outSubresources, std::vector<uint8_t>& _outFileData,
                                  size_t _maxSize = 0, DdsAlphaMode* _outAlphaMode = nullptr, bool* _outIsCubeMap = nullptr);

      // Extended version with additional options
      static HRESULT LoadFromMemoryEx(ID3D12Device* _device, const uint8_t* _ddsData, size_t _ddsDataSize, size_t _maxSize,
                                      D3D12_RESOURCE_FLAGS _resourceFlags, DdsLoaderFlags _loaderFlags, ID3D12Resource** _outTexture,
                                      std::vector<D3D12_SUBRESOURCE_DATA>& _outSubresources, DdsAlphaMode* _outAlphaMode = nullptr,
                                      bool* _outIsCubeMap = nullptr);

      // Extended version from file with additional options
      // _outFileData receives ownership of the file buffer - keep alive until upload completes
      static HRESULT LoadFromFileEx(ID3D12Device* _device, const std::wstring& _filePath, size_t _maxSize,
                                    D3D12_RESOURCE_FLAGS _resourceFlags, DdsLoaderFlags _loaderFlags, ID3D12Resource** _outTexture,
                                    std::vector<D3D12_SUBRESOURCE_DATA>& _outSubresources, std::vector<uint8_t>& _outFileData,
                                    DdsAlphaMode* _outAlphaMode = nullptr, bool* _outIsCubeMap = nullptr);

    private:
      // Prevent instantiation
      DdsTextureLoader() = delete;
  };
} // namespace Neuron::Graphics
