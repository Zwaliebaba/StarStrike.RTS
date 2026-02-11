#include "pch.h"
#include "FrameUploadAllocator.h"
#include "GraphicsCore.h"

namespace Neuron::Graphics
{

void FrameUploadAllocator::Startup(size_t _ringBufferSize)
{
  if (sm_initialized)
    return;

  sm_bufferSize = _ringBufferSize;

  auto device = Core::GetD3DDevice();
  auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(_ringBufferSize);

  check_hresult(device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &bufferDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(sm_ringBuffer.put())));

  sm_ringBuffer->SetName(L"Frame Upload Ring Buffer");

  check_hresult(sm_ringBuffer->Map(0, nullptr, reinterpret_cast<void**>(&sm_mappedPtr)));

  sm_currentOffset = 0;
  for (UINT i = 0; i < MAX_FRAMES; ++i)
    sm_frameStartOffset[i] = 0;

  sm_initialized = true;

  DebugTrace("FrameUploadAllocator::Startup - Ring buffer size: {} MB\n", 
             _ringBufferSize / (1024 * 1024));
}

void FrameUploadAllocator::Shutdown()
{
  if (!sm_initialized)
    return;

  if (sm_ringBuffer && sm_mappedPtr)
  {
    sm_ringBuffer->Unmap(0, nullptr);
    sm_mappedPtr = nullptr;
  }

  sm_ringBuffer = nullptr;
  sm_bufferSize = 0;
  sm_currentOffset = 0;
  sm_initialized = false;

  DebugTrace("FrameUploadAllocator::Shutdown\n");
}

void FrameUploadAllocator::OnFrameComplete(UINT _frameIndex)
{
  // Record where the next frame's allocations will start
  // This allows us to detect if we're about to overwrite data still in use
  UINT nextFrame = (_frameIndex + 1) % MAX_FRAMES;
  sm_frameStartOffset[nextFrame] = sm_currentOffset.load();
}

FrameUploadAllocator::Allocation FrameUploadAllocator::Allocate(size_t _size, size_t _alignment)
{
  DEBUG_ASSERT_TEXT(sm_initialized, "FrameUploadAllocator not initialized\n");
  DEBUG_ASSERT_TEXT(_size > 0, "Cannot allocate 0 bytes\n");
  DEBUG_ASSERT_TEXT((_alignment & (_alignment - 1)) == 0, "Alignment must be power of 2\n");

  // Atomic allocation with alignment
  size_t currentOffset;
  size_t alignedOffset;
  size_t newOffset;

  do
  {
    currentOffset = sm_currentOffset.load(std::memory_order_relaxed);
    
    // Align the offset
    alignedOffset = (currentOffset + _alignment - 1) & ~(_alignment - 1);
    newOffset = alignedOffset + _size;

    // Check for wrap-around
    if (newOffset > sm_bufferSize)
    {
      // Wrap to start of buffer
      alignedOffset = 0;
      newOffset = _size;
      
      // Safety check: ensure we're not overwriting in-flight data
      // In a proper implementation, we'd check against oldest frame's start offset
      DEBUG_ASSERT_TEXT(newOffset <= sm_bufferSize, 
                        "Allocation too large for ring buffer\n");
    }
  } while (!sm_currentOffset.compare_exchange_weak(currentOffset, newOffset,
                                                    std::memory_order_release,
                                                    std::memory_order_relaxed));

  Allocation alloc;
  alloc.gpuAddress = sm_ringBuffer->GetGPUVirtualAddress() + alignedOffset;
  alloc.cpuAddress = sm_mappedPtr + alignedOffset;

  return alloc;
}

} // namespace Neuron::Graphics
