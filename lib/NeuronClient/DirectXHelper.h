#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

#define D3DX12_NO_STATE_OBJECT_HELPERS
#include "d3dx12.h"

#include "Debug.h"
#include "MathCommon.h"

#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "dxguid.lib")

#define IID_GRAPHICS_PPV_ARGS(ppType)       __uuidof(ppType), (ppType).put_void()

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL      ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN   ((D3D12_GPU_VIRTUAL_ADDRESS)-1)

constexpr D3D12_CPU_DESCRIPTOR_HANDLE D3D12_CPU_HANDLE_NULL = {D3D12_GPU_VIRTUAL_ADDRESS_NULL};
constexpr D3D12_CPU_DESCRIPTOR_HANDLE D3D12_CPU_HANDLE_UNKNOWN = {D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN};

constexpr D3D12_GPU_DESCRIPTOR_HANDLE D3D12_GPU_HANDLE_NULL = {D3D12_GPU_VIRTUAL_ADDRESS_NULL};
constexpr D3D12_GPU_DESCRIPTOR_HANDLE D3D12_GPU_HANDLE_UNKNOWN = {D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN};

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
                ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))
#endif

consteval uint32_t FIXED_MSG(const char* _msg)
{
  return static_cast<uint32_t>(_msg[0]) << 24 | static_cast<uint32_t>(_msg[1]) << 16 | static_cast<uint32_t>(_msg[2]) << 8 | static_cast<
    uint32_t>(_msg[3]);
}

#if defined _DEBUG
inline void SetName(ID3D12Object* _pObject, const LPCWSTR _name) { check_hresult(_pObject->SetName(_name)); }

inline void SetNameIndexed(ID3D12Object* _pObject, const LPCWSTR _name, const UINT _index)
{
  WCHAR fullName[50];
  if (swprintf_s(fullName, L"%s[%u]", _name, _index) > 0)
    check_hresult(_pObject->SetName(fullName));
}

#else
inline void SetName([[maybe_unused]] ID3D12Object* _pObject, [[maybe_unused]] const LPCWSTR _name) {}
inline void SetNameIndexed([[maybe_unused]] ID3D12Object* _pObject, [[maybe_unused]] const LPCWSTR _name, [[maybe_unused]] const UINT _index) {}
#endif

#define NAME_D3D12_OBJECT(x) SetName((x).get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].get(), L#x, n)

class GpuUploadBuffer
{
  public:
    [[nodiscard]] ID3D12Resource* GetResource() const { return m_resource.get(); }
    virtual void Release() { m_resource = nullptr; }
    [[nodiscard]] UINT64 Size() const { return m_resource.get() ? m_resource->GetDesc().Width : 0; }

  protected:
    com_ptr<ID3D12Resource> m_resource;

    GpuUploadBuffer() = default;

    ~GpuUploadBuffer()
    {
      if (m_resource.get())
        m_resource->Unmap(0, nullptr);
    }

    void Allocate(ID3D12Device5* device, size_t bufferSize, LPCWSTR resourceName = nullptr)
    {
      auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

      auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
      check_hresult(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COMMON,
                                                    nullptr, IID_GRAPHICS_PPV_ARGS(m_resource)));
      if (resourceName)
        m_resource->SetName(resourceName);
    }

    uint8_t* MapCpuWriteOnly() const
    {
      uint8_t* mappedData;
      // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
      CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
      check_hresult(m_resource->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));
      return mappedData;
    }
};

// Helper class to create and update a constant buffer with proper constant buffer alignments.
// Usage: 
//    ConstantBuffer<...> cb;
//    cb.Create(...);
//    cb.staging.var = ... ; | cb->var = ... ; 
//    cb.CopyStagingToGpu(...);
//    Set...View(..., cb.GpuVirtualAddress());
template <class T>
class ConstantBuffer : public GpuUploadBuffer
{
  static_assert(sizeof(T) % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0,
                "Constant buffer structs must be 256-byte aligned for root CBV usage.");

  uint8_t* m_mappedConstantData;
  size_t m_alignedInstanceSize;
  size_t m_numInstances;

  public:
    ConstantBuffer()
      : m_mappedConstantData(nullptr),
        m_alignedInstanceSize(0),
        m_numInstances(0) {}

    virtual ~ConstantBuffer() = default;

    void Create(ID3D12Device5* device, UINT numInstances = 1, LPCWSTR resourceName = nullptr)
    {
      m_numInstances = numInstances;
      m_alignedInstanceSize = AlignUp(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
      size_t bufferSize = numInstances * m_alignedInstanceSize;
      Allocate(device, bufferSize, resourceName);
      m_mappedConstantData = MapCpuWriteOnly();
    }

    void CopyStagingToGpu(size_t instanceIndex = 0)
    {
      DEBUG_ASSERT(m_mappedConstantData != nullptr);
      memcpy(m_mappedConstantData + instanceIndex * m_alignedInstanceSize, &staging, sizeof(T));
    }

    // Accessors
    T staging;
    T* operator->() { return &staging; }
    [[nodiscard]] size_t NumInstances() const { return m_numInstances; }

    [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress(size_t instanceIndex = 0) const
    {
      return m_resource->GetGPUVirtualAddress() + instanceIndex * m_alignedInstanceSize;
    }
};

// Helper class to create and update a structured buffer.
// Usage: 
//    StructuredBuffer<...> sb;
//    sb.Create(...);
//    sb[index].var = ... ; 
//    sb.CopyStagingToGpu(...);
//    Set...View(..., sb.GpuVirtualAddress());
template <class T>
class StructuredBuffer : public GpuUploadBuffer
{
  T* m_mappedBuffers;
  std::vector<T> m_staging;
  size_t m_numInstances;

  public:
    // Performance tip: Align structures on sizeof(float4) boundary.
    // Ref: https://developer.nvidia.com/content/understanding-structured-buffer-performance
    static_assert(sizeof(T) % 16 == 0, "Align structure buffers on 16 byte boundary for performance reasons.");

    StructuredBuffer()
      : m_mappedBuffers(nullptr),
        m_numInstances(0) {}

    virtual ~StructuredBuffer() = default;

    void Create(ID3D12Device5* device, size_t numElements, size_t numInstances = 1, LPCWSTR resourceName = nullptr)
    {
      m_numInstances = numInstances;
      m_staging.resize(numElements);
      size_t bufferSize = numInstances * numElements * sizeof(T);
      Allocate(device, bufferSize, resourceName);
      m_mappedBuffers = reinterpret_cast<T*>(MapCpuWriteOnly());
    }

    void CopyStagingToGpu(size_t instanceIndex = 0)
    {
      DEBUG_ASSERT(m_mappedBuffers != nullptr);
      if (m_staging.empty())
        return;
      memcpy(m_mappedBuffers + instanceIndex * NumElements(), m_staging.data(), InstanceSize());
    }

    auto begin() { return m_staging.begin(); }
    auto end() { return m_staging.end(); }
    auto begin() const { return m_staging.begin(); }
    auto end() const { return m_staging.end(); }

    // Accessors
    T& operator[](size_t elementIndex) { return m_staging[elementIndex]; }
    const T& operator[](size_t elementIndex) const { return m_staging[elementIndex]; }
    [[nodiscard]] size_t NumElements() const { return m_staging.size(); }
    [[nodiscard]] static size_t ElementSize() { return sizeof(T); }
    [[nodiscard]] size_t NumInstances() const { return m_numInstances; }
    [[nodiscard]] size_t InstanceSize() const { return NumElements() * ElementSize(); }

    [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress(UINT instanceIndex = 0, UINT elementIndex = 0) const
    {
      return m_resource->GetGPUVirtualAddress() + instanceIndex * InstanceSize() + elementIndex * ElementSize();
    }
};

//--------------------------------------------------------------------------------------
// TextureUploader - Synchronously uploads texture subresource data to GPU
//
// Usage:
//    ID3D12Resource* texture = ...;  // Created texture in COMMON or COPY_DEST state
//    std::vector<D3D12_SUBRESOURCE_DATA> subresources = ...;
//    HRESULT hr = UploadTextureData(device, commandQueue, texture, subresources);
//
// Note: This function blocks until upload is complete. The texture will be in
//       PIXEL_SHADER_RESOURCE state after successful upload.
//--------------------------------------------------------------------------------------
inline HRESULT UploadTextureData(
  ID3D12Device* _device,
  ID3D12CommandQueue* _commandQueue,
  ID3D12Resource* _texture,
  const std::vector<D3D12_SUBRESOURCE_DATA>& _subresources,
  D3D12_RESOURCE_STATES _initialState = D3D12_RESOURCE_STATE_COMMON,
  D3D12_RESOURCE_STATES _finalState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
{
  if (!_device || !_commandQueue || !_texture || _subresources.empty())
    return E_INVALIDARG;

  HRESULT hr = S_OK;

  // Create command allocator
  ID3D12CommandAllocator* cmdAlloc = nullptr;
  hr = _device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
  if (FAILED(hr))
    return hr;

  // Create command list
  ID3D12GraphicsCommandList* cmdList = nullptr;
  hr = _device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, nullptr, IID_PPV_ARGS(&cmdList));
  if (FAILED(hr))
  {
    cmdAlloc->Release();
    return hr;
  }

  // Calculate upload buffer size
  const UINT64 uploadBufferSize = GetRequiredIntermediateSize(_texture, 0, static_cast<UINT>(_subresources.size()));

  // Create upload buffer
  ID3D12Resource* uploadBuffer = nullptr;
  CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

  hr = _device->CreateCommittedResource(
    &uploadHeapProps,
    D3D12_HEAP_FLAG_NONE,
    &uploadBufferDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&uploadBuffer));

  if (FAILED(hr))
  {
    cmdList->Release();
    cmdAlloc->Release();
    return hr;
  }

  // Transition to copy destination if needed
  if (_initialState != D3D12_RESOURCE_STATE_COPY_DEST)
  {
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      _texture, _initialState, D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &barrier);
  }

  // Upload subresources
  UpdateSubresources(cmdList, _texture, uploadBuffer, 0, 0,
                     static_cast<UINT>(_subresources.size()), _subresources.data());

  // Transition to final state if needed
  if (_finalState != D3D12_RESOURCE_STATE_COPY_DEST)
  {
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      _texture, D3D12_RESOURCE_STATE_COPY_DEST, _finalState);
    cmdList->ResourceBarrier(1, &barrier);
  }

  // Close and execute command list
  cmdList->Close();
  ID3D12CommandList* cmdLists[] = {cmdList};
  _commandQueue->ExecuteCommandLists(1, cmdLists);

  // Create fence and wait for completion
  ID3D12Fence* fence = nullptr;
  hr = _device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
  if (FAILED(hr))
  {
    uploadBuffer->Release();
    cmdList->Release();
    cmdAlloc->Release();
    return hr;
  }

  HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!fenceEvent)
  {
    fence->Release();
    uploadBuffer->Release();
    cmdList->Release();
    cmdAlloc->Release();
    return HRESULT_FROM_WIN32(GetLastError());
  }

  _commandQueue->Signal(fence, 1);
  if (fence->GetCompletedValue() < 1)
  {
    fence->SetEventOnCompletion(1, fenceEvent);
    WaitForSingleObject(fenceEvent, INFINITE);
  }

  // Cleanup
  CloseHandle(fenceEvent);
  fence->Release();
  uploadBuffer->Release();
  cmdList->Release();
  cmdAlloc->Release();

  return S_OK;
}
