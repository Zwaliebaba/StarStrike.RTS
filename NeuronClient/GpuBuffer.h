#pragma once

class GpuBuffer : public GpuResource
{
public:
  GpuBuffer()
  {
    m_ResourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    m_UAV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
    m_SRV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
  }
  ~GpuBuffer() override { GpuResource::Destroy(); }
  void Create(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize);

  const D3D12_CPU_DESCRIPTOR_HANDLE& GetUAV() const { return m_UAV; }
  const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV() const { return m_SRV; }

  D3D12_GPU_VIRTUAL_ADDRESS RootConstantBufferView() const { return m_gpuVirtualAddress; }

  size_t GetBufferSize() const { return m_BufferSize; }
  uint32_t GetElementCount() const { return m_ElementCount; }
  uint32_t GetElementSize() const { return m_ElementSize; }

  D3D12_VERTEX_BUFFER_VIEW VertexBufferView(size_t _offset, uint32_t _numElements = UINT_MAX, uint32_t _stride = UINT_MAX) const
  {
    D3D12_VERTEX_BUFFER_VIEW VBView;
    VBView.BufferLocation = m_gpuVirtualAddress + _offset;
    VBView.SizeInBytes = _numElements == UINT_MAX ? m_ElementCount : _numElements;
    VBView.StrideInBytes = _stride == UINT_MAX ? m_ElementSize : _stride;
    return VBView;
  }

  D3D12_INDEX_BUFFER_VIEW IndexBufferView(size_t _offset, uint32_t _size = UINT_MAX, bool _b32Bit = false) const
  {
    D3D12_INDEX_BUFFER_VIEW IBView;
    IBView.BufferLocation = m_gpuVirtualAddress + _offset;
    IBView.Format = _b32Bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    IBView.SizeInBytes = _size == UINT_MAX ? m_ElementCount * m_ElementSize : _size;
    return IBView;
  }

protected:
  D3D12_CPU_DESCRIPTOR_HANDLE m_UAV = {};
  D3D12_CPU_DESCRIPTOR_HANDLE m_SRV = {};

  size_t m_BufferSize = {};
  uint32_t m_ElementCount = {};
  uint32_t m_ElementSize = {};
  D3D12_RESOURCE_FLAGS m_ResourceFlags = {};
};

