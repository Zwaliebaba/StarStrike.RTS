#include "pch.h"
#include "ResourceStateTracker.h"

namespace Neuron::Graphics
{

void ResourceStateTracker::Reset()
{
  m_commandList = nullptr;
  m_numBarriersToFlush = 0;
}

void ResourceStateTracker::TransitionResource(GpuResource& _resource, const D3D12_RESOURCE_STATES _newState, const bool _flushImmediate)
{
  const D3D12_RESOURCE_STATES oldState = _resource.GetCurrentState();

  if (m_numBarriersToFlush == MAX_NUM_BARRIERS)
    FlushResourceBarriers();

  if (oldState != _newState)
  {
    D3D12_RESOURCE_BARRIER& barrierDesc = m_resourceBarrierBuffer[m_numBarriersToFlush++];

    barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierDesc.Transition.pResource = _resource.GetResource();
    barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrierDesc.Transition.StateBefore = oldState;
    barrierDesc.Transition.StateAfter = _newState;

    // Insert UAV barrier on SRV<->UAV transitions.
    if (oldState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS || _newState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
      InsertUAVBarrier(_resource);

    // Check to see if we already started the transition
    if (_newState == _resource.GetTransitioningState())
    {
      barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
      _resource.m_transitioningState = static_cast<D3D12_RESOURCE_STATES>(-1);
    }
    else
      barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

    _resource.m_usageState = _newState;
  }
  else if (_newState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    InsertUAVBarrier(_resource);

  if (_flushImmediate)
    FlushResourceBarriers();
}

void ResourceStateTracker::BeginResourceTransition(GpuResource& _resource, D3D12_RESOURCE_STATES _newState, bool _flushImmediate)
{
  if (m_numBarriersToFlush == MAX_NUM_BARRIERS)
    FlushResourceBarriers();

  // If it's already transitioning, finish that transition
  if (_resource.GetTransitioningState() != static_cast<D3D12_RESOURCE_STATES>(-1))
    TransitionResource(_resource, _resource.GetTransitioningState());

  const D3D12_RESOURCE_STATES oldState = _resource.m_usageState;

  if (oldState != _newState)
  {
    D3D12_RESOURCE_BARRIER& barrierDesc = m_resourceBarrierBuffer[m_numBarriersToFlush++];

    barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierDesc.Transition.pResource = _resource.GetResource();
    barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrierDesc.Transition.StateBefore = oldState;
    barrierDesc.Transition.StateAfter = _newState;
    barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;

    _resource.m_transitioningState = _newState;
  }

  if (_flushImmediate || m_numBarriersToFlush == MAX_NUM_BARRIERS)
    FlushResourceBarriers();
}

void ResourceStateTracker::InsertUAVBarrier(GpuResource& _resource, bool _flushImmediate)
{
  if (m_numBarriersToFlush == MAX_NUM_BARRIERS)
    FlushResourceBarriers();

  D3D12_RESOURCE_BARRIER& barrierDesc = m_resourceBarrierBuffer[m_numBarriersToFlush++];

  barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrierDesc.UAV.pResource = _resource.GetResource();

  if (_flushImmediate)
    FlushResourceBarriers();
}

void ResourceStateTracker::FlushResourceBarriers()
{
  if (m_numBarriersToFlush > 0)
  {
    m_commandList->ResourceBarrier(m_numBarriersToFlush, m_resourceBarrierBuffer);
    m_numBarriersToFlush = 0;
  }
}

} // namespace Neuron::Graphics
