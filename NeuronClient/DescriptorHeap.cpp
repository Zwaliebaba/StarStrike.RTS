#include "pch.h"
#include "DescriptorHeap.h"

using namespace Graphics;

//
// DescriptorHeap implementation
//

void DescriptorHeap::Create(const std::wstring& Name, D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t MaxCount, bool _forceCPU)
{
  bool gpuHeap;
  m_HeapDesc.Type = Type;
  m_HeapDesc.NumDescriptors = MaxCount;
  if ((Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) && !_forceCPU)
  {
    m_HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    gpuHeap = true;
  }
  else
  {
    m_HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    gpuHeap = false;
  }

  m_HeapDesc.NodeMask = 1;

  check_hresult(Core::GetD3DDevice()->CreateDescriptorHeap(&m_HeapDesc, IID_GRAPHICS_PPV_ARGS(m_Heap)));
  SetName(m_Heap.get(), Name.c_str());

  m_DescriptorSize = Core::GetD3DDevice()->GetDescriptorHandleIncrementSize(m_HeapDesc.Type);
  m_NumFreeDescriptors = m_HeapDesc.NumDescriptors;

  if (gpuHeap)
    m_FirstHandle = DescriptorHandle(m_Heap->GetCPUDescriptorHandleForHeapStart(), m_Heap->GetGPUDescriptorHandleForHeapStart());
  else
    m_FirstHandle = DescriptorHandle(m_Heap->GetCPUDescriptorHandleForHeapStart(), D3D12_GPU_HANDLE_NULL);

  m_NextFreeHandle = m_FirstHandle;
}

DescriptorHandle DescriptorHeap::Alloc(uint32_t Count)
{
  DEBUG_ASSERT_TEXT(HasAvailableSpace(Count), "Descriptor Heap out of space.  Increase heap size.");
  DescriptorHandle ret = m_NextFreeHandle;
  m_NextFreeHandle += Count * m_DescriptorSize;
  m_NumFreeDescriptors -= Count;
  return ret;
}

bool DescriptorHeap::ValidateHandle(const DescriptorHandle& DHandle) const
{
  if (DHandle.GetCpuPtr() < m_FirstHandle.GetCpuPtr() || DHandle.GetCpuPtr() >= m_FirstHandle.GetCpuPtr() + m_HeapDesc.NumDescriptors *
    m_DescriptorSize)
    return false;

  if (DHandle.GetGpuPtr() - m_FirstHandle.GetGpuPtr() != DHandle.GetCpuPtr() - m_FirstHandle.GetCpuPtr())
    return false;

  return true;
}

void DescriptorAllocator::DestroyAll()
{
  for (auto& desc : sm_descriptorHeapPool)
    desc.Destroy();
}

void DescriptorAllocator::Create()
{
  sm_descriptorHeapPool[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].Create(L"CBV_SRV_UAV Descriptor Heap", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NUM_DESCRIPTORS_PER_HEAP);
  sm_descriptorHeapPool[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].Create(L"Sampler Descriptor Heap", D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, NUM_DESCRIPTORS_PER_HEAP);
  sm_descriptorHeapPool[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].Create(L"RTV Descriptor Heap", D3D12_DESCRIPTOR_HEAP_TYPE_RTV, Core::GetBackBufferCount());
  sm_descriptorHeapPool[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].Create(L"DSV Descriptor Heap", D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
  sm_descriptorHeapPool[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES].Create(L"CBV_SRV_UAV CPU Descriptor Heap", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NUM_DESCRIPTORS_PER_HEAP);
}

DescriptorHandle DescriptorAllocator::Allocate(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t _count, bool _forceCpu)
{
  if (_forceCpu && (Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER))
    Type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;

  return sm_descriptorHeapPool[Type].Alloc(_count);
}

void DescriptorAllocator::SetDescriptorHeaps(ID3D12GraphicsCommandList* _commandlist)
{
  ID3D12DescriptorHeap* heapPtr[2] = {
    sm_descriptorHeapPool[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].GetHeapPointer(),
    sm_descriptorHeapPool[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].GetHeapPointer()
  };
  _commandlist->SetDescriptorHeaps(2, heapPtr);
}
