#include "pch.h"
#include "GraphicsCore.h"
#include "GraphicsCommon.h"
#include "FrameUploadAllocator.h"

using namespace Neuron::Graphics;

namespace
{
  DXGI_FORMAT NoSRGB(DXGI_FORMAT fmt) noexcept
  {
    switch (fmt)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
      return DXGI_FORMAT_B8G8R8X8_UNORM;
    default:
      return fmt;
    }
  }
}

void Core::Startup(DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthBufferFormat, UINT backBufferCount, D3D_FEATURE_LEVEL minFeatureLevel,
                   unsigned flags) noexcept(false)
{
  m_backBufferFormat = backBufferFormat;
  m_depthBufferFormat = depthBufferFormat;
  m_backBufferCount = backBufferCount;
  m_d3dMinFeatureLevel = minFeatureLevel;
  m_options = flags;

  DEBUG_ASSERT_TEXT(backBufferCount > 1 && backBufferCount <= MAX_BACK_BUFFER_COUNT, "invalid backBufferCount");
  DEBUG_ASSERT_TEXT(minFeatureLevel >= D3D_FEATURE_LEVEL_12_0, "invalid minFeatureLevel");

  CreateDeviceResources();
}

void Core::Shutdown()
{
  WaitForGpu();

  ReleaseWindowSizeDependentResources();
  ReleaseDeviceResources();

#ifdef _DEBUG
  {
    com_ptr<IDXGIDebug1> dxgiDebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_GRAPHICS_PPV_ARGS(dxgiDebug))))
    {
      dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL,
                                   static_cast<DXGI_DEBUG_RLO_FLAGS>(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
    }
  }
#endif
}

// Configures the Direct3D device, and stores handles to it and the device context.
void Core::CreateDeviceResources()
{
#if defined(_DEBUG)
  // Enable the debug layer (requires the Graphics Tools "optional feature").
  //
  // NOTE: Enabling the debug layer after device creation will invalidate the active device.
  {
    com_ptr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_GRAPHICS_PPV_ARGS(debugController))))
      debugController->EnableDebugLayer();
    else
      OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");

    com_ptr<IDXGIInfoQueue> dxgiInfoQueue;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_GRAPHICS_PPV_ARGS(dxgiInfoQueue))))
    {
      m_dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

      dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
      dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

      DXGI_INFO_QUEUE_MESSAGE_ID hide[] = {
        80 /* IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides. */,
      };
      DXGI_INFO_QUEUE_FILTER filter = {};
      filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
      filter.DenyList.pIDList = hide;
      dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
    }
  }
#endif

  check_hresult(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_GRAPHICS_PPV_ARGS(m_dxgiFactory)));

  // Determines whether tearing support is available for fullscreen borderless windows.
  if (m_options & ALLOW_TEARING)
  {
    BOOL allowTearing = FALSE;
    HRESULT hr = m_dxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
    if (FAILED(hr) || !allowTearing)
    {
      m_options &= ~ALLOW_TEARING;
      DebugTrace("WARNING: Variable refresh rate displays not supported");
    }
  }

  com_ptr<IDXGIAdapter1> adapter;
  GetAdapter(adapter.put());

  // Create the DX12 API device object.
  HRESULT hr = D3D12CreateDevice(adapter.get(), m_d3dMinFeatureLevel, IID_GRAPHICS_PPV_ARGS(m_d3dDevice));
  check_hresult(hr);

  m_d3dDevice->SetName(L"Core");

#ifndef NDEBUG
  // Configure debug device (if active).
  com_ptr<ID3D12InfoQueue> d3dInfoQueue;
  m_d3dDevice.as(d3dInfoQueue);

#ifdef _DEBUG
  d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
  d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif
  D3D12_MESSAGE_ID hide[] = {
    D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE, D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
    D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE
  };
  D3D12_INFO_QUEUE_FILTER filter = {};
  filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
  filter.DenyList.pIDList = hide;
  d3dInfoQueue->AddStorageFilterEntries(&filter);
#endif

  // Determine maximum supported feature level for this device
  static constexpr D3D_FEATURE_LEVEL FEATURE_LEVELS[] = {
    D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
  };

  D3D12_FEATURE_DATA_FEATURE_LEVELS featLevels = {static_cast<UINT>(std::size(FEATURE_LEVELS)), FEATURE_LEVELS, D3D_FEATURE_LEVEL_11_0};

  hr = m_d3dDevice->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels));
  if (SUCCEEDED(hr))
    m_d3dFeatureLevel = featLevels.MaxSupportedFeatureLevel;
  else
    m_d3dFeatureLevel = m_d3dMinFeatureLevel;

  // Create the command queue.
  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

  check_hresult(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_GRAPHICS_PPV_ARGS(m_commandQueue)));

  m_commandQueue->SetName(L"Core");

  // Create a command allocator for each back buffer that will be rendered to.
  for (UINT n = 0; n < m_backBufferCount; n++)
  {
    check_hresult(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_GRAPHICS_PPV_ARGS(m_commandAllocators[n])));

    wchar_t name[25] = {};
    swprintf_s(name, L"Render target %u", n);
    m_commandAllocators[n]->SetName(name);
  }

  // Create a command list for recording graphics commands.
  check_hresult(m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].get(), nullptr,
                                               IID_GRAPHICS_PPV_ARGS(m_commandList)));
  check_hresult(m_commandList->Close());
  m_openCommandList = false;

  m_commandList->SetName(L"Core");

  sm_gpuResourceStateTracker.Bind(m_commandList.get());

  // Create a fence for tracking GPU execution progress.
  check_hresult(m_d3dDevice->CreateFence(m_fenceValues[m_backBufferIndex], D3D12_FENCE_FLAG_NONE, IID_GRAPHICS_PPV_ARGS(m_fence)));
  m_fenceValues[m_backBufferIndex]++;

  m_fence->SetName(L"Core");

  m_fenceEvent.attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
  check_bool(static_cast<bool>(m_fenceEvent));

  DescriptorAllocator::Create();
  FrameUploadAllocator::Startup(m_backBufferCount);

  // Common state was moved to GraphicsCommon.*
  InitializeCommonState();
}

void Core::ReleaseDeviceResources()
{
  FrameUploadAllocator::Shutdown();
  DestroyCommonState();

  m_fenceEvent.close();
  m_fence = nullptr;
  m_swapChain = nullptr;
  m_d3dDevice = nullptr;
  m_dxgiFactory = nullptr;
}

// These resources need to be recreated every time the window size is changed.
void Core::CreateWindowSizeDependentResources()
{
  if (!m_window)
    throw std::logic_error("Call SetWindow with a valid Win32 window handle");

  // Wait until all previous GPU work is complete.
  WaitForGpu();

  // Release resources that are tied to the swap chain and update fence values.
  for (UINT n = 0; n < m_backBufferCount; n++)
  {
    m_renderTargets[n].Destroy();
    m_fenceValues[n] = m_fenceValues[m_backBufferIndex];
  }

  // Determine the render target size in pixels.
  const UINT backBufferWidth = std::max<UINT>(static_cast<UINT>(m_outputSize.right - m_outputSize.left), 1u);
  const UINT backBufferHeight = std::max<UINT>(static_cast<UINT>(m_outputSize.bottom - m_outputSize.top), 1u);
  const DXGI_FORMAT backBufferFormat = NoSRGB(m_backBufferFormat);

  // If the swap chain already exists, resize it, otherwise create one.
  if (m_swapChain)
  {
    // If the swap chain already exists, resize it.
    HRESULT hr = m_swapChain->ResizeBuffers(m_backBufferCount, backBufferWidth, backBufferHeight, backBufferFormat,
                                            (m_options & ALLOW_TEARING) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u);

    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
#ifdef _DEBUG
      char buff[64] = {};
      sprintf_s(buff, "Device Lost on ResizeBuffers: Reason code 0x%08X\n",
                static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED) ? m_d3dDevice->GetDeviceRemovedReason() : hr));
      OutputDebugStringA(buff);
#endif
      // If the device was removed for any reason, a new device and swap chain will need to be created.
      HandleDeviceLost();

      // Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method
      // and correctly set up the new device.
      return;
    }
    check_hresult(hr);
  }
  else
  {
    // Create a descriptor for the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = backBufferWidth;
    swapChainDesc.Height = backBufferHeight;
    swapChainDesc.Format = backBufferFormat;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = m_backBufferCount;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapChainDesc.Flags = (m_options & ALLOW_TEARING) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
    fsSwapChainDesc.Windowed = TRUE;

    // Create a swap chain for the window.
    com_ptr<IDXGISwapChain1> swapChain;
    check_hresult(m_dxgiFactory->CreateSwapChainForHwnd(m_commandQueue.get(), m_window, &swapChainDesc, &fsSwapChainDesc, nullptr,
                                                        swapChain.put()));

    swapChain.as(m_swapChain);

    // This class does not support exclusive full-screen mode and prevents DXGI from responding to the ALT+ENTER shortcut
    check_hresult(m_dxgiFactory->MakeWindowAssociation(m_window, DXGI_MWA_NO_ALT_ENTER));
  }

  // Handle color space settings for HDR
  UpdateColorSpace();

  // Obtain the back buffers for this window which will be the final render targets
  // and create render target views for each of them.
  for (UINT n = 0; n < m_backBufferCount; n++)
  {
    check_hresult(m_swapChain->GetBuffer(n, IID_PPV_ARGS(m_renderTargets[n].Put())));

   // Swap chain buffers are always in PRESENT/COMMON state initially.
   m_renderTargets[n].SetResourceState(D3D12_RESOURCE_STATE_PRESENT);

    wchar_t name[25] = {};
    swprintf_s(name, L"Render target %u", n);
    m_renderTargets[n]->SetName(name);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = m_backBufferFormat;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    if (m_rtvDescriptors[n].ptr == 0)
      m_rtvDescriptors[n] = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_d3dDevice->CreateRenderTargetView(m_renderTargets[n].GetResource(), &rtvDesc, m_rtvDescriptors[n]);
  }

  // Reset the index to the current back buffer.
  m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

  if (m_depthBufferFormat != DXGI_FORMAT_UNKNOWN)
  {
    // Allocate a 2-D surface as the depth/stencil buffer and create a depth/stencil view
    // on this surface.
    CD3DX12_HEAP_PROPERTIES depthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_depthBufferFormat, backBufferWidth, backBufferHeight, 1,
                                                                        // This depth stencil view has only one texture.
                                                                        1);
    depthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = m_depthBufferFormat;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    check_hresult(m_d3dDevice->CreateCommittedResource(&depthHeapProperties, D3D12_HEAP_FLAG_NONE, &depthStencilDesc,
                                                       D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue,
                                                       IID_PPV_ARGS(m_depthStencil.Put())));

    m_depthStencil->SetName(L"Depth stencil");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = m_depthBufferFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    if (m_dsvDescriptor.ptr == 0)
      m_dsvDescriptor = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_d3dDevice->CreateDepthStencilView(m_depthStencil.GetResource(), &dsvDesc, m_dsvDescriptor);
  }

  // Set the 3D rendering viewport and scissor rectangle to target the entire window.
  m_screenViewport.TopLeftX = m_screenViewport.TopLeftY = 0.f;
  m_screenViewport.Width = static_cast<float>(backBufferWidth);
  m_screenViewport.Height = static_cast<float>(backBufferHeight);
  m_screenViewport.MinDepth = D3D12_MIN_DEPTH;
  m_screenViewport.MaxDepth = D3D12_MAX_DEPTH;

  m_scissorRect.left = m_scissorRect.top = 0;
  m_scissorRect.right = static_cast<LONG>(backBufferWidth);
  m_scissorRect.bottom = static_cast<LONG>(backBufferHeight);
}

void Core::ReleaseWindowSizeDependentResources()
{
  DescriptorAllocator::DestroyAll();

  for (UINT n = 0; n < m_backBufferCount; n++)
  {
    m_commandAllocators[n] = nullptr;
    m_renderTargets[n].Destroy();
  }

  m_depthStencil.Destroy();
  m_commandQueue = nullptr;
  m_commandList = nullptr;
  sm_gpuResourceStateTracker.Reset();
}

// This method is called when the Win32 window is created (or re-created).
void Core::SetWindow(HWND window, int width, int height) noexcept
{
  m_window = window;
  WindowSizeChanged(width, height);
}

// This method is called when the Win32 window changes size.
bool Core::WindowSizeChanged(int width, int height)
{
  RECT newRc;
  newRc.left = newRc.top = 0;
  newRc.right = width;
  newRc.bottom = height;
  if (newRc.left == m_outputSize.left && newRc.top == m_outputSize.top && newRc.right == m_outputSize.right && newRc.bottom == m_outputSize.
    bottom)
  {
    // Handle color space settings for HDR
    UpdateColorSpace();

    return false;
  }

  m_outputSize = newRc;
  CreateWindowSizeDependentResources();
  return true;
}

// Recreate all device resources and set them back to the current state.
void Core::HandleDeviceLost()
{
  if (m_deviceNotify)
    m_deviceNotify->OnDeviceLost();

  ReleaseWindowSizeDependentResources();
  ReleaseDeviceResources();

#ifdef _DEBUG
  {
    com_ptr<IDXGIDebug1> dxgiDebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
      dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL,
                                   static_cast<DXGI_DEBUG_RLO_FLAGS>(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
  }
#endif

  CreateDeviceResources();
  CreateWindowSizeDependentResources();

  if (m_deviceNotify)
    m_deviceNotify->OnDeviceRestored();
}

// Prepare the command list and render target for rendering.
void Core::Prepare()
{
  ResetCommandAllocatorAndCommandlist();

  // Record where this frame's ring buffer allocations start
  FrameUploadAllocator::BeginFrame(m_backBufferIndex);

  DescriptorAllocator::SetDescriptorHeaps(m_commandList.get());

  sm_gpuResourceStateTracker.TransitionResource(m_renderTargets[m_backBufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
}

// Present the contents of the swap chain to the screen.
void Core::Present()
{
  sm_gpuResourceStateTracker.TransitionResource(m_renderTargets[m_backBufferIndex], D3D12_RESOURCE_STATE_PRESENT, true);
  ExecuteCommandList();

  HRESULT hr;
  if (m_options & ALLOW_TEARING)
  {
    // Recommended to always use tearing if supported when using a sync interval of 0.
    // Note this will fail if in true 'fullscreen' mode.
    hr = m_swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
  }
  else
  {
    // The first argument instructs DXGI to block until VSync, putting the application
    // to sleep until the next VSync. This ensures we don't waste any cycles rendering
    // frames that will never be displayed to the screen.
    hr = m_swapChain->Present(1, 0);
  }

  // If the device was reset we must completely reinitialize the renderer.
  if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
  {
    DebugTrace("Device Lost on Present: Reason code 0x{:08X}\n",
              static_cast<unsigned int>((hr == DXGI_ERROR_DEVICE_REMOVED) ? m_d3dDevice->GetDeviceRemovedReason() : hr));
    HandleDeviceLost();
  }
  else
  {
    check_hresult(hr);

    MoveToNextFrame();

    if (!m_dxgiFactory->IsCurrent())
    {
      // Output information is cached on the DXGI Factory. If it is stale we need to create a new factory.
      check_hresult(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_GRAPHICS_PPV_ARGS(m_dxgiFactory)));
    }
  }
}

// Wait for pending GPU work to complete.
void Core::WaitForGpu() noexcept
{
  if (m_commandQueue && m_fence)
  {
    // Schedule a Signal command in the GPU queue.
    UINT64 fenceValue = m_fenceValues[m_backBufferIndex];
    if (SUCCEEDED(m_commandQueue->Signal(m_fence.get(), fenceValue)))
    {
      // Wait until the Signal has been processed.
      if (SUCCEEDED(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent.get())))
      {
        WaitForSingleObjectEx(m_fenceEvent.get(), INFINITE, FALSE);

        // Increment the fence value for the current frame.
        m_fenceValues[m_backBufferIndex]++;
      }
    }
  }
}

void Core::ResetCommandAllocatorAndCommandlist()
{
  // Close the command list if it's still open
  if (m_openCommandList)
  {
    check_hresult(m_commandList->Close());
    m_openCommandList = false;
  }

  // Reset command list and allocator.
  check_hresult(m_commandAllocators[m_backBufferIndex]->Reset());
  check_hresult(m_commandList->Reset(m_commandAllocators[m_backBufferIndex].get(), nullptr));
  m_openCommandList = true;
}

void Core::ExecuteCommandList(bool force)
{
  if (m_openCommandList || force)
  {
    check_hresult(m_commandList->Close());
    ID3D12CommandList* commandLists[] = { m_commandList.get() };
    m_commandQueue->ExecuteCommandLists(ARRAYSIZE(commandLists), commandLists);

    m_openCommandList = false;
  }
}

// Prepare to render the next frame.
void Core::MoveToNextFrame()
{
  // Schedule a Signal command in the queue.
  const UINT64 currentFenceValue = m_fenceValues[m_backBufferIndex];
  check_hresult(m_commandQueue->Signal(m_fence.get(), currentFenceValue));

  // Update the back buffer index.
  m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

  // If the next frame is not ready to be rendered yet, wait until it is ready.
  if (m_fence->GetCompletedValue() < m_fenceValues[m_backBufferIndex])
  {
    check_hresult(m_fence->SetEventOnCompletion(m_fenceValues[m_backBufferIndex], m_fenceEvent.get()));
    WaitForSingleObjectEx(m_fenceEvent.get(), INFINITE, FALSE);
  }

  // Notify upload allocator that this frame's GPU work is complete
  FrameUploadAllocator::OnFrameComplete(m_backBufferIndex);

  // Set the fence value for the next frame.
  m_fenceValues[m_backBufferIndex] = currentFenceValue + 1;
}


// This method acquires the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, try WARP. Otherwise throw an exception.
void Core::GetAdapter(IDXGIAdapter1** ppAdapter)
{
  *ppAdapter = nullptr;

  com_ptr<IDXGIAdapter1> adapter;
  for (UINT adapterIndex = 0; SUCCEEDED(
         m_dxgiFactory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_GRAPHICS_PPV_ARGS(adapter)));
       adapterIndex++)
  {
    DXGI_ADAPTER_DESC1 desc;
    check_hresult(adapter->GetDesc1(&desc));

    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
    {
      // Don't select the Basic Render Driver adapter.
      continue;
    }

    // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
    if (SUCCEEDED(D3D12CreateDevice(adapter.get(), m_d3dMinFeatureLevel, _uuidof(ID3D12Device), nullptr)))
    {
#ifdef _DEBUG
      wchar_t buff[256] = {};
      swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterIndex, desc.VendorId, desc.DeviceId, desc.Description);
      OutputDebugStringW(buff);
#endif
      break;
    }
  }

#if !defined(NDEBUG)
  if (!adapter)
  {
    // Try WARP12 instead
    if (FAILED(m_dxgiFactory->EnumWarpAdapter(IID_GRAPHICS_PPV_ARGS(adapter))))
      throw std::runtime_error("WARP12 not available. Enable the 'Graphics Tools' optional feature");

    OutputDebugStringA("Direct3D Adapter - WARP12\n");
  }
#endif

  if (!adapter)
    throw std::runtime_error("No Direct3D 12 device found");

  *ppAdapter = adapter.detach();
}

// Sets the color space for the swap chain in order to handle HDR output.
void Core::UpdateColorSpace()
{
  DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

  bool isDisplayHDR10 = false;

  if (m_swapChain)
  {
    com_ptr<IDXGIOutput> output;
    if (SUCCEEDED(m_swapChain->GetContainingOutput(output.put())))
    {
      com_ptr<IDXGIOutput6> output6;
      output.as(output6);

      DXGI_OUTPUT_DESC1 desc;
      check_hresult(output6->GetDesc1(&desc));

      if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
      {
        // Display output is HDR10.
        isDisplayHDR10 = true;
      }
    }
  }

  if ((m_options & ENABLE_HDR) && isDisplayHDR10)
  {
    switch (m_backBufferFormat)
    {
    case DXGI_FORMAT_R10G10B10A2_UNORM:
      // The application creates the HDR10 signal.
      colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
      break;

    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      // The system creates the HDR10 signal; application uses linear values.
      colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
      break;

    default:
      break;
    }
  }

  m_colorSpace = colorSpace;

  UINT colorSpaceSupport = 0;
  if (SUCCEEDED(m_swapChain->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport)) && (colorSpaceSupport &
    DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
    check_hresult(m_swapChain->SetColorSpace1(colorSpace));
}
