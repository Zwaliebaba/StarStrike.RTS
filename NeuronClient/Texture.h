#pragma once

#include "GpuResource.h"
#include "DescriptorHeap.h"

namespace Neuron::Graphics
{
  // GPU texture resource wrapper for DDS textures
  class Texture : public GpuResource
  {
  public:
    Texture() = default;
    ~Texture() override { Texture::Destroy(); }

    // Non-copyable
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Movable
    Texture(Texture&& _other) noexcept;
    Texture& operator=(Texture&& _other) noexcept;

    void Destroy() override;

    // Accessors
    [[nodiscard]] uint32_t GetWidth() const { return m_width; }
    [[nodiscard]] uint32_t GetHeight() const { return m_height; }
    [[nodiscard]] uint32_t GetMipLevels() const { return m_mipLevels; }
    [[nodiscard]] DXGI_FORMAT GetFormat() const { return m_format; }
    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return m_srvHandle; }
    [[nodiscard]] bool IsValid() const { return GetResource() != nullptr; }
    [[nodiscard]] bool IsCubeMap() const { return m_isCubeMap; }

  private:
    friend class TextureManager;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_mipLevels = 0;
    DXGI_FORMAT m_format = DXGI_FORMAT_UNKNOWN;
    bool m_isCubeMap = false;
    DescriptorHandle m_srvHandle;
  };
}
