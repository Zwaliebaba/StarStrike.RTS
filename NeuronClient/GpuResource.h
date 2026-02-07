#pragma once

class GpuResource
{
  friend class ResourceStateTracker;

  public:
    GpuResource()
      : m_usageState(D3D12_RESOURCE_STATE_COMMON),
        m_transitioningState(static_cast<D3D12_RESOURCE_STATES>(-1)),
        m_gpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL) {}

    GpuResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES CurrentState)
      : m_usageState(CurrentState),
        m_transitioningState(static_cast<D3D12_RESOURCE_STATES>(-1)),
        m_gpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL) { m_pResource.attach(pResource); }

    void Set(ID3D12Resource* pResource, D3D12_RESOURCE_STATES CurrentState)
    {
      m_pResource.attach(pResource);
      m_usageState = CurrentState;
      m_transitioningState = static_cast<D3D12_RESOURCE_STATES>(-1);
    }
    void SetResourceState(D3D12_RESOURCE_STATES CurrentState) { m_usageState = CurrentState; }

    virtual ~GpuResource() { GpuResource::Destroy(); }

    virtual void Destroy()
    {
      m_pResource = nullptr;
      m_gpuVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
      ++m_VersionID;
    }

    auto GetCurrentState() const { return m_usageState; }
    auto GetTransitioningState() const { return m_transitioningState; }

    ID3D12Resource* operator->() { return m_pResource.get(); }
    const ID3D12Resource* operator->() const { return m_pResource.get(); }

    ID3D12Resource* GetResource() { return m_pResource.get(); }
    const ID3D12Resource* GetResource() const { return m_pResource.get(); }

    ID3D12Resource** Put() { return m_pResource.put(); }

    D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() const { return m_gpuVirtualAddress; }

    uint32_t GetVersionID() const { return m_VersionID; }

  protected:
    com_ptr<ID3D12Resource> m_pResource;
    D3D12_RESOURCE_STATES m_usageState;
    D3D12_RESOURCE_STATES m_transitioningState;
    D3D12_GPU_VIRTUAL_ADDRESS m_gpuVirtualAddress;

    // Used to identify when a resource changes so descriptors can be copied etc.
    uint32_t m_VersionID = 0;
};
