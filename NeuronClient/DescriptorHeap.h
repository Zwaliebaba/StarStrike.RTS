#pragma once

namespace Neuron::Graphics
{
  // This handle refers to a descriptor or a descriptor table (contiguous descriptors) that is shader visible.
  class DescriptorHandle
  {
  public:
    DescriptorHandle()
    {
      m_CpuHandle = D3D12_CPU_HANDLE_UNKNOWN;
      m_GpuHandle = D3D12_GPU_HANDLE_UNKNOWN;
    }

    DescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle)
      : m_CpuHandle(CpuHandle), m_GpuHandle(GpuHandle)
    {
    }

    DescriptorHandle operator+ (INT OffsetScaledByDescriptorSize) const
    {
      DescriptorHandle ret = *this;
      ret += OffsetScaledByDescriptorSize;
      return ret;
    }

    void operator += (INT OffsetScaledByDescriptorSize)
    {
      if (m_CpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
        m_CpuHandle.ptr += OffsetScaledByDescriptorSize;
      if (m_GpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
        m_GpuHandle.ptr += OffsetScaledByDescriptorSize;
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE* operator&() const { return &m_CpuHandle; }
    operator D3D12_CPU_DESCRIPTOR_HANDLE() const { return m_CpuHandle; }
    operator D3D12_GPU_DESCRIPTOR_HANDLE() const { return m_GpuHandle; }

    size_t GetCpuPtr() const { return m_CpuHandle.ptr; }
    uint64_t GetGpuPtr() const { return m_GpuHandle.ptr; }
    bool IsNull() const { return m_CpuHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }
    bool IsShaderVisible() const { return m_GpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }

  private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_CpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_GpuHandle;
  };

  class DescriptorHeap
  {
  public:

    DescriptorHeap() = default;
    ~DescriptorHeap() { Destroy(); }

    void Create(const std::wstring& DebugHeapName, D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t MaxCount, bool _forceCPU = false);
    void Destroy() { m_Heap = nullptr; }

    bool HasAvailableSpace(uint32_t Count) const { return Count <= m_NumFreeDescriptors; }
    DescriptorHandle Alloc(uint32_t Count = 1);

    DescriptorHandle operator[] (uint32_t arrayIdx) const { return m_FirstHandle + arrayIdx * m_DescriptorSize; }

    uint32_t GetOffsetOfHandle(const DescriptorHandle& DHandle)
    {
      return (uint32_t)(DHandle.GetCpuPtr() - m_FirstHandle.GetCpuPtr()) / m_DescriptorSize;
    }

    bool ValidateHandle(const DescriptorHandle& DHandle) const;

    ID3D12DescriptorHeap* GetHeapPointer() const { return m_Heap.get(); }

    uint32_t GetDescriptorSize() const { return m_DescriptorSize; }

  private:

    com_ptr<ID3D12DescriptorHeap> m_Heap;
    D3D12_DESCRIPTOR_HEAP_DESC m_HeapDesc;
    uint32_t m_DescriptorSize;
    uint32_t m_NumFreeDescriptors;
    DescriptorHandle m_FirstHandle;
    DescriptorHandle m_NextFreeHandle;
  };

  class DescriptorAllocator
  {
  public:
    static void Create();
    static DescriptorHandle Allocate(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t _count, bool _forceCpu = false);

    static void SetDescriptorHeaps(ID3D12GraphicsCommandList* _commandlist);
    static void DestroyAll();

  protected:

    static constexpr uint32_t NUM_DESCRIPTORS_PER_HEAP = 256;
    inline static std::mutex sm_allocationMutex;
    inline static std::array<DescriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES+1> sm_descriptorHeapPool;
  };
}
