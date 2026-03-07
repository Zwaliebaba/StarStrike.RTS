#include "pch.h"
#include "GpuBuffer.h"

void GpuBuffer::Create(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize)
{
  auto device = Graphics::Core::GetD3DDevice();

  Destroy();

  m_ElementCount = NumElements;
  m_ElementSize = ElementSize;
  m_BufferSize = NumElements * ElementSize;

  m_usageState = D3D12_RESOURCE_STATE_COMMON;

  const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

  auto indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_BufferSize);
  check_hresult(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &indexBufferDesc,
    D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_pResource)));

  m_gpuVirtualAddress = m_pResource->GetGPUVirtualAddress();
  check_hresult(m_pResource->SetName(name.c_str()));
}
