#pragma once

#include "GpuResource.h"

class ResourceStateTracker
{
public:
  void Bind(ID3D12GraphicsCommandList7* _commandList) { m_commandList = _commandList; }
  void Reset();

  void TransitionResource(GpuResource& _resource, D3D12_RESOURCE_STATES _newState, bool _flushImmediate = false);
  void BeginResourceTransition(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
  void InsertUAVBarrier(GpuResource& _resource, bool _flushImmediate = false);
  void FlushResourceBarriers();

protected:
  ID3D12GraphicsCommandList7* m_commandList{};
  static constexpr UINT MAX_NUM_BARRIERS = 16;
  D3D12_RESOURCE_BARRIER m_ResourceBarrierBuffer[MAX_NUM_BARRIERS]{};
  UINT m_numBarriersToFlush = 0;
};
