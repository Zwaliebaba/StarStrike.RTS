#include "pch.h"
#include "TextureManager.h"
#include "DDSTextureLoader.h"
#include "GraphicsCore.h"

namespace Neuron::Graphics
{
  void TextureManager::Startup()
  {
    DebugTrace("TextureManager::Startup\n");
  }

  void TextureManager::Shutdown()
  {
    DebugTrace("TextureManager::Shutdown - Releasing {} cached textures\n", sm_textureCache.size());
    ReleaseAllTextures();
  }

  Texture* TextureManager::LoadFromFile(const std::wstring& _filePath)
  {
    std::lock_guard<std::mutex> lock(sm_cacheMutex);

    // Check cache first
    auto it = sm_textureCache.find(_filePath);
    if (it != sm_textureCache.end())
    {
      return it->second.get();
    }

    // Create new texture
    auto texture = std::make_unique<Texture>();

    if (!LoadDDSFromFile(_filePath, *texture))
    {
      DebugTrace("TextureManager::LoadFromFile - Failed to load: {}\n",
                 std::string(_filePath.begin(), _filePath.end()));
      return nullptr;
    }

    Texture* result = texture.get();
    sm_textureCache[_filePath] = std::move(texture);

    DebugTrace("TextureManager::LoadFromFile - Loaded: {} ({}x{}, {} mips)\n",
               std::string(_filePath.begin(), _filePath.end()),
               result->GetWidth(), result->GetHeight(), result->GetMipLevels());

    return result;
  }

  Texture* TextureManager::LoadFromMemory(const void* _data, size_t _size, const std::wstring& _name)
  {
    std::lock_guard<std::mutex> lock(sm_cacheMutex);

    // Check cache first
    auto it = sm_textureCache.find(_name);
    if (it != sm_textureCache.end())
    {
      return it->second.get();
    }

    // Create new texture
    auto texture = std::make_unique<Texture>();

    if (!LoadDDSFromMemory(_data, _size, *texture))
    {
      DebugTrace("TextureManager::LoadFromMemory - Failed to load: {}\n",
                 std::string(_name.begin(), _name.end()));
      return nullptr;
    }

    Texture* result = texture.get();
    sm_textureCache[_name] = std::move(texture);

    DebugTrace("TextureManager::LoadFromMemory - Loaded: {} ({}x{}, {} mips)\n",
               std::string(_name.begin(), _name.end()),
               result->GetWidth(), result->GetHeight(), result->GetMipLevels());

    return result;
  }

  Texture* TextureManager::GetTexture(const std::wstring& _name)
  {
    std::lock_guard<std::mutex> lock(sm_cacheMutex);

    auto it = sm_textureCache.find(_name);
    if (it != sm_textureCache.end())
    {
      return it->second.get();
    }
    return nullptr;
  }

  void TextureManager::ReleaseTexture(const std::wstring& _name)
  {
    std::lock_guard<std::mutex> lock(sm_cacheMutex);

    auto it = sm_textureCache.find(_name);
    if (it != sm_textureCache.end())
    {
      DebugTrace("TextureManager::ReleaseTexture - Releasing: {}\n",
                 std::string(_name.begin(), _name.end()));
      sm_textureCache.erase(it);
    }
  }

  void TextureManager::ReleaseAllTextures()
  {
    std::lock_guard<std::mutex> lock(sm_cacheMutex);
    sm_textureCache.clear();
  }

  size_t TextureManager::GetCacheSize()
  {
    std::lock_guard<std::mutex> lock(sm_cacheMutex);
    return sm_textureCache.size();
  }

  bool TextureManager::LoadDDSFromFile(const std::wstring& _filePath, Texture& _outTexture)
  {
    auto device = Core::GetD3DDevice();
    if (!device)
    {
      DebugTrace("TextureManager::LoadDDSFromFile - No D3D device available\n");
      return false;
    }

    std::unique_ptr<uint8_t[]> ddsData;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    ID3D12Resource* textureResource = nullptr;
    bool isCubeMap = false;

    HRESULT hr = LoadDDSTextureFromFile(
      device,
      _filePath.c_str(),
      &textureResource,
      ddsData,
      subresources,
      0,
      nullptr,
      &isCubeMap);

    if (FAILED(hr))
    {
      DebugTrace("TextureManager::LoadDDSFromFile - LoadDDSTextureFromFile failed: 0x{:08X}\n", static_cast<uint32_t>(hr));
      return false;
    }

    // Attach resource to Texture's GpuResource base
    _outTexture.Set(textureResource, D3D12_RESOURCE_STATE_COPY_DEST);
    _outTexture.m_isCubeMap = isCubeMap;

    // Get texture description for metadata
    auto desc = textureResource->GetDesc();
    _outTexture.m_width = static_cast<uint32_t>(desc.Width);
    _outTexture.m_height = desc.Height;
    _outTexture.m_mipLevels = desc.MipLevels;
    _outTexture.m_format = desc.Format;

    // Upload texture data to GPU
    if (!UploadTexture(_outTexture, subresources))
    {
      _outTexture.Destroy();
      return false;
    }

    // Create SRV
    if (!CreateSRV(_outTexture))
    {
      _outTexture.Destroy();
      return false;
    }

    return true;
  }

  bool TextureManager::LoadDDSFromMemory(const void* _data, size_t _size, Texture& _outTexture)
  {
    auto device = Core::GetD3DDevice();
    if (!device)
    {
      DebugTrace("TextureManager::LoadDDSFromMemory - No D3D device available\n");
      return false;
    }

    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    ID3D12Resource* textureResource = nullptr;
    bool isCubeMap = false;

    HRESULT hr = LoadDDSTextureFromMemory(
      device,
      static_cast<const uint8_t*>(_data),
      _size,
      &textureResource,
      subresources,
      0,
      nullptr,
      &isCubeMap);

    if (FAILED(hr))
    {
      DebugTrace("TextureManager::LoadDDSFromMemory - LoadDDSTextureFromMemory failed: 0x{:08X}\n", static_cast<uint32_t>(hr));
      return false;
    }

    // Attach resource to Texture's GpuResource base
    _outTexture.Set(textureResource, D3D12_RESOURCE_STATE_COPY_DEST);
    _outTexture.m_isCubeMap = isCubeMap;

    // Get texture description for metadata
    auto desc = textureResource->GetDesc();
    _outTexture.m_width = static_cast<uint32_t>(desc.Width);
    _outTexture.m_height = desc.Height;
    _outTexture.m_mipLevels = desc.MipLevels;
    _outTexture.m_format = desc.Format;

    // Upload texture data to GPU
    if (!UploadTexture(_outTexture, subresources))
    {
      _outTexture.Destroy();
      return false;
    }

    // Create SRV
    if (!CreateSRV(_outTexture))
    {
      _outTexture.Destroy();
      return false;
    }

    return true;
  }

  bool TextureManager::UploadTexture(Texture& _texture, const std::vector<D3D12_SUBRESOURCE_DATA>& _subresources)
  {
    auto device = Core::GetD3DDevice();
    auto commandQueue = Core::GetCommandQueue();
    auto resource = _texture.GetResource();

    if (!device || !commandQueue || !resource || _subresources.empty())
    {
      return false;
    }

    // Use the DirectXHelper upload function
    HRESULT hr = UploadTextureData(
      device,
      commandQueue,
      resource,
      _subresources,
      D3D12_RESOURCE_STATE_COPY_DEST,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    if (FAILED(hr))
    {
      DebugTrace("TextureManager::UploadTexture - UploadTextureData failed: 0x{:08X}\n", static_cast<uint32_t>(hr));
      return false;
    }

    // Update tracked state
    _texture.SetResourceState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    return true;
  }

  bool TextureManager::CreateSRV(Texture& _texture)
  {
    auto device = Core::GetD3DDevice();
    auto resource = _texture.GetResource();

    if (!device || !resource)
    {
      return false;
    }

    auto desc = resource->GetDesc();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if (_texture.IsCubeMap())
    {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
      srvDesc.TextureCube.MipLevels = desc.MipLevels;
      srvDesc.TextureCube.MostDetailedMip = 0;
    }
    else if (desc.DepthOrArraySize > 1 && desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
    {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
      srvDesc.Texture2DArray.MipLevels = desc.MipLevels;
      srvDesc.Texture2DArray.MostDetailedMip = 0;
      srvDesc.Texture2DArray.FirstArraySlice = 0;
      srvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
    }
    else if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
      srvDesc.Texture3D.MipLevels = desc.MipLevels;
      srvDesc.Texture3D.MostDetailedMip = 0;
    }
    else if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
    {
      if (desc.DepthOrArraySize > 1)
      {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
        srvDesc.Texture1DArray.MipLevels = desc.MipLevels;
        srvDesc.Texture1DArray.MostDetailedMip = 0;
        srvDesc.Texture1DArray.FirstArraySlice = 0;
        srvDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
      }
      else
      {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
        srvDesc.Texture1D.MipLevels = desc.MipLevels;
        srvDesc.Texture1D.MostDetailedMip = 0;
      }
    }
    else
    {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Texture2D.MipLevels = desc.MipLevels;
      srvDesc.Texture2D.MostDetailedMip = 0;
    }

    _texture.m_srvHandle = Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CreateShaderResourceView(resource, &srvDesc, _texture.m_srvHandle);

    return true;
  }
}
