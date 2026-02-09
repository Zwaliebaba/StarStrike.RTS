#include "pch.h"
#include "Texture.h"

namespace Neuron::Graphics
{
  Texture::Texture(Texture&& _other) noexcept
    : GpuResource(std::move(_other)),
      m_width(_other.m_width),
      m_height(_other.m_height),
      m_mipLevels(_other.m_mipLevels),
      m_format(_other.m_format),
      m_isCubeMap(_other.m_isCubeMap),
      m_srvHandle(_other.m_srvHandle)
  {
    _other.m_width = 0;
    _other.m_height = 0;
    _other.m_mipLevels = 0;
    _other.m_format = DXGI_FORMAT_UNKNOWN;
    _other.m_isCubeMap = false;
    _other.m_srvHandle = {};
  }

  Texture& Texture::operator=(Texture&& _other) noexcept
  {
    if (this != &_other)
    {
      Destroy();

      GpuResource::operator=(std::move(_other));

      m_width = _other.m_width;
      m_height = _other.m_height;
      m_mipLevels = _other.m_mipLevels;
      m_format = _other.m_format;
      m_isCubeMap = _other.m_isCubeMap;
      m_srvHandle = _other.m_srvHandle;

      _other.m_width = 0;
      _other.m_height = 0;
      _other.m_mipLevels = 0;
      _other.m_format = DXGI_FORMAT_UNKNOWN;
      _other.m_isCubeMap = false;
      _other.m_srvHandle = {};
    }
    return *this;
  }

  void Texture::Destroy()
  {
    GpuResource::Destroy();
    m_width = 0;
    m_height = 0;
    m_mipLevels = 0;
    m_format = DXGI_FORMAT_UNKNOWN;
    m_isCubeMap = false;
    m_srvHandle = {};
  }
}
