#pragma once

namespace Neuron::Graphics
{
  /// Ring-buffer allocator for per-frame dynamic vertex/constant data.
  /// Ensures GPU reads from previous frames don't conflict with CPU writes.
  class FrameUploadAllocator
  {
  public:
     static void Startup(UINT _backBufferCount, size_t _ringBufferSize = 8 * 1024 * 1024);
    static void Shutdown();

    /// Called by Core::MoveToNextFrame() after fence wait completes.
    /// Marks the boundary for the next frame's allocations.
    static void OnFrameComplete(UINT _frameIndex);

    /// Called at the start of each frame to record where this frame's allocations begin.
    /// Must be called before any Allocate() calls for the frame.
    static void BeginFrame(UINT _frameIndex);

    /// Allocation result with both CPU and GPU addresses
    struct Allocation
    {
      D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;
      void* cpuAddress;
    };

    /// Allocate memory from the per-frame ring buffer segment.
    /// Thread-safe within a single frame (uses atomic offset).
    /// @param _size Size in bytes to allocate
    /// @param _alignment Alignment requirement (default 16 bytes)
    /// @return Allocation with valid CPU/GPU addresses
    [[nodiscard]] static Allocation Allocate(size_t _size, size_t _alignment = 16);

    /// Get current frame's allocation offset (for debugging)
    [[nodiscard]] static size_t GetCurrentOffset() noexcept { return sm_currentOffset; }

    /// Get total buffer size
    [[nodiscard]] static size_t GetBufferSize() noexcept { return sm_bufferSize; }

    /// Queue an upload buffer to be released after GPU completes current frame's work.
    /// This allows texture/buffer uploads without blocking on WaitForGpu().
    static void DeferUploadBufferRelease(com_ptr<ID3D12Resource> _uploadBuffer);

  private:
    static constexpr UINT MAX_FRAMES = 3;

    inline static com_ptr<ID3D12Resource> sm_ringBuffer;
    inline static uint8_t* sm_mappedPtr = nullptr;
    inline static size_t sm_bufferSize = 0;
    inline static std::atomic<size_t> sm_currentOffset{0};
    inline static size_t sm_frameStartOffset[MAX_FRAMES] = {};
    inline static bool sm_initialized = false;
    inline static std::atomic<UINT> sm_completedFrames{0};
    inline static UINT sm_frameCount = 2;
    inline static UINT sm_currentFrameIndex = 0;

    /// Pending upload buffers waiting for GPU to finish using them
    struct PendingUpload
    {
      com_ptr<ID3D12Resource> buffer;
      UINT frameIndex;
    };
    inline static std::vector<PendingUpload> sm_pendingUploads;
    inline static std::mutex sm_pendingUploadsMutex;
  };

} // namespace Neuron::Graphics
