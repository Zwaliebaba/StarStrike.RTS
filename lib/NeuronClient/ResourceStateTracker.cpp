#include "pch.h"
#include "ResourceStateTracker.h"

void ResourceStateTracker::Reset()
{
  m_commandList = nullptr;
  m_numBarriersToFlush = 0;
}

void ResourceStateTracker::TransitionResource(GpuResource& _resource, const D3D12_RESOURCE_STATES _newState, const bool _flushImmediate)
{
  const D3D12_RESOURCE_STATES OldState = _resource.GetCurrentState();

  if (m_numBarriersToFlush == MAX_NUM_BARRIERS)
    FlushResourceBarriers();

  if (OldState != _newState)
  {
    D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_numBarriersToFlush++];

    BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    BarrierDesc.Transition.pResource = _resource.GetResource();
    BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    BarrierDesc.Transition.StateBefore = OldState;
    BarrierDesc.Transition.StateAfter = _newState;

    // Insert UAV barrier on SRV<->UAV transitions.
    if (OldState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS || _newState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
      InsertUAVBarrier(_resource);

    // Check to see if we already started the transition
    if (_newState == _resource.GetTransitioningState())
    {
      BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
      _resource.m_transitioningState = static_cast<D3D12_RESOURCE_STATES>(-1);
    }
    else
      BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

    _resource.m_usageState = _newState;
  }
  else if (_newState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    InsertUAVBarrier(_resource);

  if (_flushImmediate)
    FlushResourceBarriers();
}

void ResourceStateTracker::BeginResourceTransition(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate)
{
  if (m_numBarriersToFlush == MAX_NUM_BARRIERS)
    FlushResourceBarriers();

  // If it's already transitioning, finish that transition
  if (Resource.GetTransitioningState() != static_cast<D3D12_RESOURCE_STATES>(-1))
    TransitionResource(Resource, Resource.GetTransitioningState());

  const D3D12_RESOURCE_STATES OldState = Resource.m_usageState;

  if (OldState != NewState)
  {
    D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_numBarriersToFlush++];

    BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    BarrierDesc.Transition.pResource = Resource.GetResource();
    BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    BarrierDesc.Transition.StateBefore = OldState;
    BarrierDesc.Transition.StateAfter = NewState;
    BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;

    Resource.m_transitioningState = NewState;
  }

  if (FlushImmediate || m_numBarriersToFlush == MAX_NUM_BARRIERS)
    FlushResourceBarriers();
}

void ResourceStateTracker::InsertUAVBarrier(GpuResource& _resource, bool _flushImmediate)
{
  if (m_numBarriersToFlush == MAX_NUM_BARRIERS)
    FlushResourceBarriers();

  D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_numBarriersToFlush++];

  BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  BarrierDesc.UAV.pResource = _resource.GetResource();

  if (_flushImmediate)
    FlushResourceBarriers();
}

void ResourceStateTracker::FlushResourceBarriers()
{
  if (m_numBarriersToFlush > 0)
  {
    m_commandList->ResourceBarrier(m_numBarriersToFlush, m_ResourceBarrierBuffer);
    m_numBarriersToFlush = 0;
  }
}
