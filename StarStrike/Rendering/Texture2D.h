#pragma once

#include "GpuResource.h"
#include "DescriptorHeap.h"

namespace Neuron { struct GdiBitmap; }

namespace StarStrike
{
  // Represents a 2D texture loaded on the GPU
  class Texture2D
  {
  public:
    Texture2D() = default;
    ~Texture2D() { Destroy(); }

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;
    Texture2D(Texture2D&&) = default;
    Texture2D& operator=(Texture2D&&) = default;

    // Create texture from raw RGBA data
    bool Create(uint32_t width, uint32_t height, const uint8_t* data, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

    // Create texture from GdiBitmap (converts palette to RGBA)
    bool CreateFromBitmap(const Neuron::GdiBitmap* bitmap);

    // Create texture from BMP file
    bool CreateFromFile(const std::wstring& filename);

    void Destroy();

    bool IsValid() const { return m_resource.GetResource() != nullptr; }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

    D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return m_srvHandle; }
   D3D12_GPU_DESCRIPTOR_HANDLE GetGpuSRV() const { return m_srvHandle; }
    ID3D12Resource* GetResource() { return m_resource.GetResource(); }

  private:
    Neuron::Graphics::GpuResource m_resource;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
   Neuron::Graphics::DescriptorHandle m_srvHandle;
  };
}
