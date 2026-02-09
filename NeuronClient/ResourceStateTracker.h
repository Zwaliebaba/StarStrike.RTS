#pragma once

#include "GpuResource.h"

namespace Neuron::Graphics
{

class ResourceStateTracker
{
public:
  void Bind(ID3D12GraphicsCommandList7* _commandList) { m_commandList = _commandList; }
  void Reset();

  void TransitionResource(GpuResource& _resource, D3D12_RESOURCE_STATES _newState, bool _flushImmediate = false);
  void BeginResourceTransition(GpuResource& _resource, D3D12_RESOURCE_STATES _newState, bool _flushImmediate = false);
  void InsertUAVBarrier(GpuResource& _resource, bool _flushImmediate = false);
  void FlushResourceBarriers();

protected:
  ID3D12GraphicsCommandList7* m_commandList{};
  static constexpr UINT MAX_NUM_BARRIERS = 16;
  D3D12_RESOURCE_BARRIER m_resourceBarrierBuffer[MAX_NUM_BARRIERS]{};
  UINT m_numBarriersToFlush = 0;
};

} // namespace Neuron::Graphics
