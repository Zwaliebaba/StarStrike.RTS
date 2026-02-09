#pragma once

#include "Texture.h"
#include <mutex>
#include <unordered_map>

namespace Neuron::Graphics
{
  // Static texture manager for loading and caching DDS textures
  class TextureManager
  {
  public:
    static void Startup();
    static void Shutdown();

    // Load DDS texture from file (cached)
    // Path is relative to FileSys::GetHomeDirectory()
    static Texture* LoadFromFile(const std::wstring& _filePath);

    // Load DDS texture from memory buffer
    static Texture* LoadFromMemory(const void* _data, size_t _size, const std::wstring& _name);

    // Retrieve cached texture by name/path
    static Texture* GetTexture(const std::wstring& _name);

    // Release specific texture from cache
    static void ReleaseTexture(const std::wstring& _name);

    // Release all cached textures
    static void ReleaseAllTextures();

    // Cache statistics
    static size_t GetCacheSize();

  private:
    inline static std::unordered_map<std::wstring, std::unique_ptr<Texture>> sm_textureCache;
    inline static std::mutex sm_cacheMutex;

    // Internal loading implementation
    static bool LoadDDSFromFile(const std::wstring& _filePath, Texture& _outTexture);
    static bool LoadDDSFromMemory(const void* _data, size_t _size, Texture& _outTexture);
    static bool CreateSRV(Texture& _texture);
    static bool UploadTexture(Texture& _texture, const std::vector<D3D12_SUBRESOURCE_DATA>& _subresources);
  };
}
