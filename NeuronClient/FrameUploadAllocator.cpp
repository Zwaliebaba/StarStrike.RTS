#include "pch.h"
#include "FrameUploadAllocator.h"
#include "GraphicsCore.h"

namespace Neuron::Graphics
{

void FrameUploadAllocator::Startup(UINT _backBufferCount, size_t _ringBufferSize)
{
  if (sm_initialized)
    return;

  sm_bufferSize = _ringBufferSize;
  sm_frameCount = _backBufferCount;

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
  for (UINT i = 0; i < sm_frameCount; ++i)
    sm_frameStartOffset[i] = 0;
  sm_completedFrames = 0;

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
  sm_completedFrames = 0;
  sm_initialized = false;

  DebugTrace("FrameUploadAllocator::Shutdown\n");
}

void FrameUploadAllocator::BeginFrame(UINT _frameIndex)
{
  // Record where this frame's allocations start
  sm_frameStartOffset[_frameIndex] = sm_currentOffset.load(std::memory_order_relaxed);
  sm_currentFrameIndex = _frameIndex;
}

void FrameUploadAllocator::OnFrameComplete(UINT _frameIndex)
{
  // Record where the next frame's allocations will start
  // This allows us to detect if we're about to overwrite data still in use
  UINT nextFrame = (_frameIndex + 1) % sm_frameCount;
  sm_frameStartOffset[nextFrame] = sm_currentOffset.load();

  // Track completed frames for overlap validation
  if (sm_completedFrames.load(std::memory_order_relaxed) < sm_frameCount)
    sm_completedFrames.fetch_add(1, std::memory_order_relaxed);

  // Release upload buffers that were queued MAX_FRAMES ago (GPU is done with them)
  {
    std::lock_guard lock(sm_pendingUploadsMutex);
    std::erase_if(sm_pendingUploads, [_frameIndex](const PendingUpload& pending) {
      // Buffer is safe to release if it was queued MAX_FRAMES-1 frames ago
      // (meaning all frames that could reference it have completed)
      UINT framesSinceQueued = (_frameIndex >= pending.frameIndex) 
        ? (_frameIndex - pending.frameIndex)
        : (sm_frameCount - pending.frameIndex + _frameIndex);
      return framesSinceQueued >= sm_frameCount - 1;
    });
  }
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

      // Safety check: ensure allocation fits in buffer
      ASSERT_TEXT(newOffset <= sm_bufferSize, 
                  L"Allocation too large for ring buffer\n");
    }

    // Validate we're not overwriting data from in-flight frames
    // The oldest in-flight frame is (currentFrame + 1) % sm_frameCount
    UINT currentFrame = Core::GetCurrentFrameIndex();
    UINT oldestFrame = (currentFrame + 1) % sm_frameCount;
    size_t oldestStart = sm_frameStartOffset[oldestFrame];
    size_t oldestEnd = sm_frameStartOffset[(oldestFrame + 1) % sm_frameCount];

    // Check if allocation would overwrite oldest frame's data
    // This happens when: we wrapped and new region overlaps oldest frame's region
    // Skip validation during initial frames before frame boundaries are properly tracked
    if (sm_completedFrames.load(std::memory_order_relaxed) >= sm_frameCount &&
        alignedOffset < oldestEnd && newOffset > oldestStart && oldestStart < oldestEnd)
    {
      // Ring buffer overrun - oldest frame's data would be corrupted
      // In production, consider waiting for GPU or using a larger buffer
      ASSERT_TEXT(false, L"Ring buffer overrun: increase buffer size or reduce per-frame allocations\n");
    }
  } while (!sm_currentOffset.compare_exchange_weak(currentOffset, newOffset,
                                                    std::memory_order_release,
                                                    std::memory_order_relaxed));

  Allocation alloc;
  alloc.gpuAddress = sm_ringBuffer->GetGPUVirtualAddress() + alignedOffset;
  alloc.cpuAddress = sm_mappedPtr + alignedOffset;

  return alloc;
}

void FrameUploadAllocator::DeferUploadBufferRelease(com_ptr<ID3D12Resource> _uploadBuffer)
{
  if (!_uploadBuffer)
    return;

  std::lock_guard lock(sm_pendingUploadsMutex);
  sm_pendingUploads.push_back({std::move(_uploadBuffer), Core::GetCurrentFrameIndex()});
}

} // namespace Neuron::Graphics
