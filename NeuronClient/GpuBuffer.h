#pragma once

#include "GpuResource.h"

namespace Neuron::Graphics
{

class GpuBuffer : public GpuResource
{
public:
  GpuBuffer()
  {
    m_resourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    m_uav.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
    m_srv.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
  }
  ~GpuBuffer() override { GpuResource::Destroy(); }
  void Create(const std::wstring& _name, uint32_t _numElements, uint32_t _elementSize);

  [[nodiscard]] const D3D12_CPU_DESCRIPTOR_HANDLE& GetUAV() const noexcept { return m_uav; }
  [[nodiscard]] const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV() const noexcept { return m_srv; }

  [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS RootConstantBufferView() const noexcept { return m_gpuVirtualAddress; }

  [[nodiscard]] size_t GetBufferSize() const noexcept { return m_bufferSize; }
  [[nodiscard]] uint32_t GetElementCount() const noexcept { return m_elementCount; }
  [[nodiscard]] uint32_t GetElementSize() const noexcept { return m_elementSize; }

  [[nodiscard]] D3D12_VERTEX_BUFFER_VIEW VertexBufferView(size_t _offset, uint32_t _numElements = UINT_MAX, uint32_t _stride = UINT_MAX) const
  {
    D3D12_VERTEX_BUFFER_VIEW vbView;
    vbView.BufferLocation = m_gpuVirtualAddress + _offset;
    vbView.SizeInBytes = _numElements == UINT_MAX ? m_elementCount : _numElements;
    vbView.StrideInBytes = _stride == UINT_MAX ? m_elementSize : _stride;
    return vbView;
  }

  [[nodiscard]] D3D12_INDEX_BUFFER_VIEW IndexBufferView(size_t _offset, uint32_t _size = UINT_MAX, bool _b32Bit = false) const
  {
    D3D12_INDEX_BUFFER_VIEW ibView;
    ibView.BufferLocation = m_gpuVirtualAddress + _offset;
    ibView.Format = _b32Bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    ibView.SizeInBytes = _size == UINT_MAX ? m_elementCount * m_elementSize : _size;
    return ibView;
  }

protected:
  D3D12_CPU_DESCRIPTOR_HANDLE m_uav = {};
  D3D12_CPU_DESCRIPTOR_HANDLE m_srv = {};

  size_t m_bufferSize = {};
  uint32_t m_elementCount = {};
  uint32_t m_elementSize = {};
  D3D12_RESOURCE_FLAGS m_resourceFlags = {};
};

} // namespace Neuron::Graphics

