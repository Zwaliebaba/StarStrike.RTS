#include "pch.h"
#include "GpuBuffer.h"

namespace Neuron::Graphics
{
  void GpuBuffer::Create(const std::wstring &_name, uint32_t _numElements, uint32_t _elementSize)
  {
    auto device = Core::GetD3DDevice();

    Destroy();

    m_elementCount = _numElements;
    m_elementSize = _elementSize;
    m_bufferSize = _numElements * _elementSize;

    m_usageState = D3D12_RESOURCE_STATE_COMMON;

    const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_bufferSize);
    check_hresult(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_pResource)));

    m_gpuVirtualAddress = m_pResource->GetGPUVirtualAddress();
    check_hresult(m_pResource->SetName(_name.c_str()));
  }
}