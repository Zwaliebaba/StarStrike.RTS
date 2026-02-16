#pragma once

namespace Neuron::Graphics
{

class GpuResource
{
  friend class ResourceStateTracker;

  public:
    GpuResource()
      : m_usageState(D3D12_RESOURCE_STATE_COMMON),
        m_transitioningState(static_cast<D3D12_RESOURCE_STATES>(-1)),
        m_gpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL) {}

    GpuResource(ID3D12Resource* _pResource, D3D12_RESOURCE_STATES _currentState)
      : m_usageState(_currentState),
        m_transitioningState(static_cast<D3D12_RESOURCE_STATES>(-1)),
        m_gpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL) { m_pResource.attach(_pResource); }

    // Non-copyable
    GpuResource(const GpuResource&) = delete;
    GpuResource& operator=(const GpuResource&) = delete;

    // Move constructor
    GpuResource(GpuResource&& _other) noexcept
      : m_pResource(std::move(_other.m_pResource)),
        m_usageState(_other.m_usageState),
        m_transitioningState(_other.m_transitioningState),
        m_gpuVirtualAddress(_other.m_gpuVirtualAddress),
        m_versionID(_other.m_versionID)
    {
      _other.m_usageState = D3D12_RESOURCE_STATE_COMMON;
      _other.m_transitioningState = static_cast<D3D12_RESOURCE_STATES>(-1);
      _other.m_gpuVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
      _other.m_versionID = 0;
    }

    // Move assignment
    GpuResource& operator=(GpuResource&& _other) noexcept
    {
      if (this != &_other)
      {
        m_pResource = std::move(_other.m_pResource);
        m_usageState = _other.m_usageState;
        m_transitioningState = _other.m_transitioningState;
        m_gpuVirtualAddress = _other.m_gpuVirtualAddress;
        m_versionID = _other.m_versionID;

        _other.m_usageState = D3D12_RESOURCE_STATE_COMMON;
        _other.m_transitioningState = static_cast<D3D12_RESOURCE_STATES>(-1);
        _other.m_gpuVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
        _other.m_versionID = 0;
      }
      return *this;
    }

    void Set(ID3D12Resource* _pResource, D3D12_RESOURCE_STATES _currentState)
    {
      m_pResource.attach(_pResource);
      m_usageState = _currentState;
      m_transitioningState = static_cast<D3D12_RESOURCE_STATES>(-1);
    }

    void SetResourceState(D3D12_RESOURCE_STATES _currentState) { m_usageState = _currentState; }

    virtual ~GpuResource() { GpuResource::Destroy(); }

    virtual void Destroy()
    {
      m_pResource = nullptr;
      m_gpuVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
     m_usageState = D3D12_RESOURCE_STATE_COMMON;
     m_transitioningState = static_cast<D3D12_RESOURCE_STATES>(-1);
      ++m_versionID;
    }

    [[nodiscard]] auto GetCurrentState() const noexcept { return m_usageState; }
    [[nodiscard]] auto GetTransitioningState() const noexcept { return m_transitioningState; }

    [[nodiscard]] ID3D12Resource* operator->() noexcept { return m_pResource.get(); }
    [[nodiscard]] const ID3D12Resource* operator->() const noexcept { return m_pResource.get(); }

    [[nodiscard]] ID3D12Resource* GetResource() noexcept { return m_pResource.get(); }
    [[nodiscard]] const ID3D12Resource* GetResource() const noexcept { return m_pResource.get(); }

    [[nodiscard]] ID3D12Resource** Put() noexcept { return m_pResource.put(); }

    [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() const noexcept { return m_gpuVirtualAddress; }

    [[nodiscard]] uint32_t GetVersionID() const noexcept { return m_versionID; }

  protected:
    com_ptr<ID3D12Resource> m_pResource;
    D3D12_RESOURCE_STATES m_usageState;
    D3D12_RESOURCE_STATES m_transitioningState;
    D3D12_GPU_VIRTUAL_ADDRESS m_gpuVirtualAddress;

    // Used to identify when a resource changes so descriptors can be copied etc.
    uint32_t m_versionID = 0;
};

} 
