#include "pch.h"
#include "Canvas.h"
#include "FontPrint.h"
#include "GraphicsCore.h"
#include "SamplerManager.h"
#include "TextureManager.h"

// Include compiled shader bytecode
#include "CompiledShaders/BasicVS.h"
#include "CompiledShaders/BasicPS.h"
#include "CompiledShaders/SpriteVS.h"
#include "CompiledShaders/SpritePS.h"

namespace Neuron::Graphics
{
  //=============================================================================
  // Lifecycle
  //=============================================================================

  void Canvas::Startup()
  {
    DebugTrace("Canvas::Startup\n");

    // Reserve capacity for batched vertices (no allocations during rendering)
    sm_lineVertices.reserve(MAX_PRIMITIVE_VERTICES);
    sm_triangleVertices.reserve(MAX_PRIMITIVE_VERTICES);
    sm_spriteVertices.reserve(MAX_SPRITE_VERTICES);
    sm_clipStack.reserve(16);
    sm_layerStack.reserve(16);

    // Create GPU resources
    CreateRootSignature();
    CreatePipelineStates();
    CreateDynamicBuffers();

    // Initialize from current backbuffer size
    RECT outputSize = Core::GetOutputSize();
    sm_physicalWidth = static_cast<uint32_t>(outputSize.right - outputSize.left);
    sm_physicalHeight = static_cast<uint32_t>(outputSize.bottom - outputSize.top);
    
    if (sm_physicalWidth == 0) sm_physicalWidth = 1920;
    if (sm_physicalHeight == 0) sm_physicalHeight = 1080;

    // Query DPI if available
    HWND hwnd = Core::GetWindow();
    if (hwnd)
    {
      UINT dpi = GetDpiForWindow(hwnd);
      sm_dpiScale = static_cast<float>(dpi) / 96.0f;
    }

    // Initialize default clip rect to full logical canvas
    sm_currentClipRect = {0, 0, static_cast<LONG>(CANVAS_LOGICAL_WIDTH), static_cast<LONG>(CANVAS_LOGICAL_HEIGHT)};

    UpdateScaling();
    UpdateProjection();

    sm_initialized = true;

    DebugTrace("Canvas initialized: logical {}x{}, physical {}x{}, DPI scale {:.2f}\n", 
               static_cast<int>(CANVAS_LOGICAL_WIDTH), static_cast<int>(CANVAS_LOGICAL_HEIGHT),
               sm_physicalWidth, sm_physicalHeight, sm_dpiScale);
  }

  void Canvas::Shutdown()
  {
    DebugTrace("Canvas::Shutdown\n");

    Core::WaitForGpu();

    // Unmap constant buffer
    if (sm_constantBuffer && sm_constantBufferMapped)
    {
      sm_constantBuffer->Unmap(0, nullptr);
      sm_constantBufferMapped = nullptr;
    }

    // Release GPU resources
    sm_constantBuffer = nullptr;

    // Clear containers
    sm_lineVertices.clear();
    sm_triangleVertices.clear();
    sm_spriteVertices.clear();
    sm_clipStack.clear();
    sm_layerStack.clear();
    sm_textures.clear();
    sm_fonts.clear();

    sm_initialized = false;
  }

  //=============================================================================
  // Configuration
  //=============================================================================

  void Canvas::Configure(AspectMode _mode, float _uiScale)
  {
    sm_aspectMode = _mode;
    sm_uiScale = _uiScale;
    UpdateScaling();
    UpdateProjection();
  }

  void Canvas::OnResize(uint32_t _width, uint32_t _height)
  {
    if (_width == 0 || _height == 0)
      return;

    sm_physicalWidth = _width;
    sm_physicalHeight = _height;

    // Update DPI if window is available
    HWND hwnd = Core::GetWindow();
    if (hwnd)
    {
      UINT dpi = GetDpiForWindow(hwnd);
      sm_dpiScale = static_cast<float>(dpi) / 96.0f;
    }

    UpdateScaling();
    UpdateProjection();

    DebugTrace("Canvas::OnResize: {}x{}, scale {:.3f}x{:.3f}, offset {:.1f},{:.1f}\n",
               _width, _height, sm_scaleX, sm_scaleY, sm_offsetX, sm_offsetY);
  }

  void Canvas::UpdateScaling()
  {
    float physicalW = static_cast<float>(sm_physicalWidth);
    float physicalH = static_cast<float>(sm_physicalHeight);

    switch (sm_aspectMode)
    {
      case AspectMode::Stretch:
        // Direct mapping, may distort
        sm_scaleX = physicalW / CANVAS_LOGICAL_WIDTH;
        sm_scaleY = physicalH / CANVAS_LOGICAL_HEIGHT;
        sm_offsetX = 0.0f;
        sm_offsetY = 0.0f;
        sm_uniformScale = 1.0f;
        break;

      case AspectMode::ScaleToFit:
      {
        // Uniform scale to fit entirely within physical bounds (letterbox/pillarbox)
        float scaleX = physicalW / CANVAS_LOGICAL_WIDTH;
        float scaleY = physicalH / CANVAS_LOGICAL_HEIGHT;
        sm_uniformScale = std::min(scaleX, scaleY);
        sm_scaleX = sm_uniformScale;
        sm_scaleY = sm_uniformScale;

        // Center the content
        float scaledWidth = CANVAS_LOGICAL_WIDTH * sm_uniformScale;
        float scaledHeight = CANVAS_LOGICAL_HEIGHT * sm_uniformScale;
        sm_offsetX = (physicalW - scaledWidth) * 0.5f;
        sm_offsetY = (physicalH - scaledHeight) * 0.5f;
        break;
      }

      case AspectMode::ScaleToFill:
      {
        // Uniform scale to fill physical bounds (may crop edges)
        float scaleX = physicalW / CANVAS_LOGICAL_WIDTH;
        float scaleY = physicalH / CANVAS_LOGICAL_HEIGHT;
        sm_uniformScale = std::max(scaleX, scaleY);
        sm_scaleX = sm_uniformScale;
        sm_scaleY = sm_uniformScale;

        // Center the content (negative offsets = cropping)
        float scaledWidth = CANVAS_LOGICAL_WIDTH * sm_uniformScale;
        float scaledHeight = CANVAS_LOGICAL_HEIGHT * sm_uniformScale;
        sm_offsetX = (physicalW - scaledWidth) * 0.5f;
        sm_offsetY = (physicalH - scaledHeight) * 0.5f;
        break;
      }

      case AspectMode::None:
        // 1:1 pixel mapping from top-left
        sm_scaleX = 1.0f;
        sm_scaleY = 1.0f;
        sm_offsetX = 0.0f;
        sm_offsetY = 0.0f;
        sm_uniformScale = 1.0f;
        break;
    }
  }

  RECT Canvas::GetVisibleLogicalRect()
  {
    // Calculate what portion of logical space is visible on screen
    float physicalW = static_cast<float>(sm_physicalWidth);
    float physicalH = static_cast<float>(sm_physicalHeight);

    // Convert physical bounds to logical
    XMFLOAT2 topLeft = PhysicalToLogical(0.0f, 0.0f);
    XMFLOAT2 bottomRight = PhysicalToLogical(physicalW, physicalH);

    // Clamp to logical bounds
    RECT result;
    result.left = static_cast<LONG>(std::max(0.0f, topLeft.x));
    result.top = static_cast<LONG>(std::max(0.0f, topLeft.y));
    result.right = static_cast<LONG>(std::min(CANVAS_LOGICAL_WIDTH, bottomRight.x));
    result.bottom = static_cast<LONG>(std::min(CANVAS_LOGICAL_HEIGHT, bottomRight.y));

    return result;
  }

  XMFLOAT2 Canvas::LogicalToPhysical(float _x, float _y)
  {
    return XMFLOAT2{
      _x * sm_scaleX + sm_offsetX,
      _y * sm_scaleY + sm_offsetY
    };
  }

  XMFLOAT2 Canvas::PhysicalToLogical(float _x, float _y)
  {
    return XMFLOAT2{
      (_x - sm_offsetX) / sm_scaleX,
      (_y - sm_offsetY) / sm_scaleY
    };
  }

  //=============================================================================
  // GPU Resource Creation
  //=============================================================================

  void Canvas::CreateRootSignature()
  {
    // Primitive root signature: CBV only
    sm_primitiveRootSig.Reset(1, 0);
    sm_primitiveRootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
    sm_primitiveRootSig.Finalize(L"Canvas Primitive RootSig",
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

    // Sprite root signature: CBV + SRV + static sampler
    sm_spriteRootSig.Reset(2, 1);
    sm_spriteRootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
    sm_spriteRootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);

    SamplerDesc spriteSampler;
    spriteSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;  // Point filtering for pixel fonts
    spriteSampler.SetTextureAddressMode(D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    sm_spriteRootSig.InitStaticSampler(0, spriteSampler, D3D12_SHADER_VISIBILITY_PIXEL);

    sm_spriteRootSig.Finalize(L"Canvas Sprite RootSig",
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);
  }

  void Canvas::CreatePipelineStates()
  {
    // Depth stencil with depth disabled for 2D
    CD3DX12_DEPTH_STENCIL_DESC noDepthDesc(D3D12_DEFAULT);
    noDepthDesc.DepthEnable = FALSE;
    noDepthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    // Line PSO
    sm_linePSO.SetRootSignature(sm_primitiveRootSig);
    sm_linePSO.SetVertexShader(g_pBasicVS, sizeof(g_pBasicVS));
    sm_linePSO.SetPixelShader(g_pBasicPS, sizeof(g_pBasicPS));
    sm_linePSO.SetInputLayout(&VertexPositionColor::INPUT_LAYOUT);
    sm_linePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
    sm_linePSO.SetRenderTargetFormat(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
    sm_linePSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
    sm_linePSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
    sm_linePSO.SetDepthStencilState(noDepthDesc);
    sm_linePSO.Finalize();

    // Triangle PSO
    sm_trianglePSO.SetRootSignature(sm_primitiveRootSig);
    sm_trianglePSO.SetVertexShader(g_pBasicVS, sizeof(g_pBasicVS));
    sm_trianglePSO.SetPixelShader(g_pBasicPS, sizeof(g_pBasicPS));
    sm_trianglePSO.SetInputLayout(&VertexPositionColor::INPUT_LAYOUT);
    sm_trianglePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    sm_trianglePSO.SetRenderTargetFormat(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
    sm_trianglePSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
    sm_trianglePSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
    sm_trianglePSO.SetDepthStencilState(noDepthDesc);
    sm_trianglePSO.Finalize();

    // Sprite PSO with alpha blending
    CD3DX12_BLEND_DESC alphaBlendDesc(D3D12_DEFAULT);
    alphaBlendDesc.RenderTarget[0].BlendEnable = TRUE;
    alphaBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    alphaBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    alphaBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    alphaBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    alphaBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    alphaBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

    sm_spritePSO.SetRootSignature(sm_spriteRootSig);
    sm_spritePSO.SetVertexShader(g_pSpriteVS, sizeof(g_pSpriteVS));
    sm_spritePSO.SetPixelShader(g_pSpritePS, sizeof(g_pSpritePS));
    sm_spritePSO.SetInputLayout(&VertexPositionTextureColor::INPUT_LAYOUT);
    sm_spritePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    sm_spritePSO.SetRenderTargetFormat(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
    sm_spritePSO.SetBlendState(alphaBlendDesc);
    sm_spritePSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
    sm_spritePSO.SetDepthStencilState(noDepthDesc);
    sm_spritePSO.Finalize();
  }

  void Canvas::CreateDynamicBuffers()
  {
    auto device = Core::GetD3DDevice();
    auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // Per-frame constant buffer (3 frames in flight, 256-byte aligned each)
    static constexpr UINT MAX_FRAMES = 3;
    size_t constantBufferSize = CONSTANT_BUFFER_FRAME_SIZE * MAX_FRAMES;
    auto constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
    check_hresult(device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE,
      &constantBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(sm_constantBuffer.put())));
    sm_constantBuffer->SetName(L"Canvas Constant Buffer (Per-Frame)");

    // Persistently map constant buffer
    check_hresult(sm_constantBuffer->Map(0, nullptr, &sm_constantBufferMapped));
  }

  D3D12_GPU_VIRTUAL_ADDRESS Canvas::GetConstantBufferGPUAddress()
  {
    UINT frameIndex = Core::GetCurrentFrameIndex();
    return sm_constantBuffer->GetGPUVirtualAddress() + (CONSTANT_BUFFER_FRAME_SIZE * frameIndex);
  }

  void Canvas::UpdateProjection()
  {
    // Orthographic projection for physical backbuffer size
    // Vertices are submitted in logical coordinates, transformed via scale/offset in projection
    float physicalW = static_cast<float>(sm_physicalWidth);
    float physicalH = static_cast<float>(sm_physicalHeight);

    // Build a combined projection that:
    // 1. Scales logical coordinates to physical
    // 2. Applies offset for letterboxing/pillarboxing
    // 3. Maps to NDC

    // We'll use physical coordinates in the projection, and transform vertices before submission
    XMMATRIX proj = XMMatrixOrthographicOffCenterLH(
      0.0f, physicalW,      // left, right (physical)
      physicalH, 0.0f,      // bottom, top (flipped for top-left origin)
      0.0f, 1.0f            // near, far
    );

    XMStoreFloat4x4(&sm_constants.Projection, XMMatrixTranspose(proj));

    // Write to current frame's constant buffer slice
    if (sm_constantBufferMapped)
    {
      UINT frameIndex = Core::GetCurrentFrameIndex();
      auto* dest = static_cast<uint8_t*>(sm_constantBufferMapped) + (CONSTANT_BUFFER_FRAME_SIZE * frameIndex);
      memcpy(dest, &sm_constants, sizeof(CanvasConstants));
    }
  }

  //=============================================================================
  // Frame Control
  //=============================================================================

  void Canvas::BeginFrame()
  {
    // Clear all batched data
    sm_lineVertices.clear();
    sm_triangleVertices.clear();
    sm_spriteVertices.clear();

    // Reset stacks
    sm_clipStack.clear();
    sm_layerStack.clear();
    sm_currentZ = 0.0f;
    sm_currentTextureIndex = -1;

    // Reset clip rect to full logical canvas
    sm_currentClipRect = {0, 0, static_cast<LONG>(CANVAS_LOGICAL_WIDTH), static_cast<LONG>(CANVAS_LOGICAL_HEIGHT)};

    // Bind the backbuffer as render target (caller should have already set it up)
    auto cmdList = Core::GetCommandList();

    // Set viewport for full physical backbuffer
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(sm_physicalWidth);
    viewport.Height = static_cast<float>(sm_physicalHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    cmdList->RSSetViewports(1, &viewport);

    // Set scissor to full physical backbuffer initially
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(sm_physicalWidth), static_cast<LONG>(sm_physicalHeight)};
    cmdList->RSSetScissorRects(1, &scissor);
  }

  void Canvas::Render()
  {
    // Flush all batched primitives to GPU
    FlushPrimitives();
    FlushSprites();
    
    // No render target transition needed - we rendered directly to backbuffer
  }

  //=============================================================================
  // Clipping Stack
  //=============================================================================

  void Canvas::PushClipRect(const RECT& _rectPixels)
  {
    // Flush current batch before changing clip
    FlushPrimitives();
    FlushSprites();

    sm_clipStack.push_back(sm_currentClipRect);

    // Intersect with current clip rect
    sm_currentClipRect.left = std::max(sm_currentClipRect.left, _rectPixels.left);
    sm_currentClipRect.top = std::max(sm_currentClipRect.top, _rectPixels.top);
    sm_currentClipRect.right = std::min(sm_currentClipRect.right, _rectPixels.right);
    sm_currentClipRect.bottom = std::min(sm_currentClipRect.bottom, _rectPixels.bottom);

    // Ensure valid scissor rect (D3D12 requires right > left and bottom > top)
    if (sm_currentClipRect.right <= sm_currentClipRect.left)
    {
      sm_currentClipRect.right = sm_currentClipRect.left + 1;
    }
    if (sm_currentClipRect.bottom <= sm_currentClipRect.top)
    {
      sm_currentClipRect.bottom = sm_currentClipRect.top + 1;
    }
  }

  void Canvas::PopClipRect()
  {
    if (sm_clipStack.empty())
    {
      DEBUG_ASSERT_TEXT(false, "Canvas::PopClipRect - Stack underflow\n");
      return;
    }

    FlushPrimitives();
    FlushSprites();

    sm_currentClipRect = sm_clipStack.back();
    sm_clipStack.pop_back();
  }

  void Canvas::ResetClipStack()
  {
    sm_clipStack.clear();
    sm_currentClipRect = {0, 0, static_cast<LONG>(CANVAS_LOGICAL_WIDTH), static_cast<LONG>(CANVAS_LOGICAL_HEIGHT)};
  }

  RECT Canvas::TransformClipRect(const RECT& _logicalRect)
  {
    // Transform logical clip rect to physical coordinates
    XMFLOAT2 topLeft = LogicalToPhysical(static_cast<float>(_logicalRect.left), 
                                          static_cast<float>(_logicalRect.top));
    XMFLOAT2 bottomRight = LogicalToPhysical(static_cast<float>(_logicalRect.right), 
                                              static_cast<float>(_logicalRect.bottom));

    RECT physicalRect;
    physicalRect.left = static_cast<LONG>(topLeft.x);
    physicalRect.top = static_cast<LONG>(topLeft.y);
    physicalRect.right = static_cast<LONG>(std::ceil(bottomRight.x));
    physicalRect.bottom = static_cast<LONG>(std::ceil(bottomRight.y));

    // Clamp to physical bounds
    physicalRect.left = std::max(0L, physicalRect.left);
    physicalRect.top = std::max(0L, physicalRect.top);
    physicalRect.right = std::min(static_cast<LONG>(sm_physicalWidth), physicalRect.right);
    physicalRect.bottom = std::min(static_cast<LONG>(sm_physicalHeight), physicalRect.bottom);

    // Ensure valid scissor rect
    if (physicalRect.right <= physicalRect.left)
      physicalRect.right = physicalRect.left + 1;
    if (physicalRect.bottom <= physicalRect.top)
      physicalRect.bottom = physicalRect.top + 1;

    return physicalRect;
  }

  void Canvas::ApplyClipRect(const RECT& _rect)
  {
    auto cmdList = Core::GetCommandList();

    // Transform logical clip rect to physical coordinates
    D3D12_RECT scissor = TransformClipRect(_rect);
    cmdList->RSSetScissorRects(1, &scissor);
  }

  //=============================================================================
  // Layer Stack
  //=============================================================================

  void Canvas::PushLayer(float _z)
  {
    sm_layerStack.push_back(sm_currentZ);
    sm_currentZ = _z;
  }

  void Canvas::PopLayer()
  {
    if (sm_layerStack.empty())
    {
      DEBUG_ASSERT_TEXT(false, "Canvas::PopLayer - Stack underflow\n");
      return;
    }

    sm_currentZ = sm_layerStack.back();
    sm_layerStack.pop_back();
  }

  //=============================================================================
  // Primitive Rendering
  //=============================================================================

  void XM_CALLCONV Canvas::DrawLine(float _x1, float _y1, float _x2, float _y2, FXMVECTOR _color)
  {
    if (sm_lineVertices.size() + 2 > MAX_PRIMITIVE_VERTICES)
    {
      FlushPrimitives();
    }

    XMFLOAT4 colorF;
    XMStoreFloat4(&colorF, _color);

    // Transform logical to physical coordinates
    XMFLOAT2 p1 = LogicalToPhysical(_x1, _y1);
    XMFLOAT2 p2 = LogicalToPhysical(_x2, _y2);

    sm_lineVertices.emplace_back(XMFLOAT3{p1.x, p1.y, sm_currentZ}, colorF);
    sm_lineVertices.emplace_back(XMFLOAT3{p2.x, p2.y, sm_currentZ}, colorF);
  }

  void XM_CALLCONV Canvas::DrawRectangle(float _left, float _top, float _right, float _bottom, FXMVECTOR _color)
  {
    if (sm_triangleVertices.size() + 6 > MAX_PRIMITIVE_VERTICES)
    {
      FlushPrimitives();
    }

    XMFLOAT4 colorF;
    XMStoreFloat4(&colorF, _color);
    float z = sm_currentZ;

    // Transform logical to physical coordinates
    XMFLOAT2 tl = LogicalToPhysical(_left, _top);
    XMFLOAT2 br = LogicalToPhysical(_right, _bottom);

    // Two triangles for the quad
    sm_triangleVertices.emplace_back(XMFLOAT3{tl.x, tl.y, z}, colorF);
    sm_triangleVertices.emplace_back(XMFLOAT3{br.x, tl.y, z}, colorF);
    sm_triangleVertices.emplace_back(XMFLOAT3{tl.x, br.y, z}, colorF);

    sm_triangleVertices.emplace_back(XMFLOAT3{br.x, tl.y, z}, colorF);
    sm_triangleVertices.emplace_back(XMFLOAT3{br.x, br.y, z}, colorF);
    sm_triangleVertices.emplace_back(XMFLOAT3{tl.x, br.y, z}, colorF);
  }

  void XM_CALLCONV Canvas::DrawRectangleOutline(float _left, float _top, float _right, float _bottom,
                                                 float _thickness, FXMVECTOR _color)
  {
    // Draw 4 rectangles for the border
    DrawRectangle(_left, _top, _right, _top + _thickness, _color);                    // Top
    DrawRectangle(_left, _bottom - _thickness, _right, _bottom, _color);              // Bottom
    DrawRectangle(_left, _top + _thickness, _left + _thickness, _bottom - _thickness, _color);  // Left
    DrawRectangle(_right - _thickness, _top + _thickness, _right, _bottom - _thickness, _color); // Right
  }

  void XM_CALLCONV Canvas::DrawTriangle(float _x1, float _y1, float _x2, float _y2,
                                         float _x3, float _y3, FXMVECTOR _color)
  {
    if (sm_triangleVertices.size() + 3 > MAX_PRIMITIVE_VERTICES)
    {
      FlushPrimitives();
    }

    XMFLOAT4 colorF;
    XMStoreFloat4(&colorF, _color);
    float z = sm_currentZ;

    // Transform logical to physical coordinates
    XMFLOAT2 p1 = LogicalToPhysical(_x1, _y1);
    XMFLOAT2 p2 = LogicalToPhysical(_x2, _y2);
    XMFLOAT2 p3 = LogicalToPhysical(_x3, _y3);

    sm_triangleVertices.emplace_back(XMFLOAT3{p1.x, p1.y, z}, colorF);
    sm_triangleVertices.emplace_back(XMFLOAT3{p2.x, p2.y, z}, colorF);
    sm_triangleVertices.emplace_back(XMFLOAT3{p3.x, p3.y, z}, colorF);
  }

  void XM_CALLCONV Canvas::DrawCircle(float _cx, float _cy, float _radius, FXMVECTOR _color, int _segments)
  {
    if (_segments < 3) _segments = 3;

    XMFLOAT4 colorF;
    XMStoreFloat4(&colorF, _color);
    float z = sm_currentZ;

    // Transform center to physical
    XMFLOAT2 center = LogicalToPhysical(_cx, _cy);
    // Scale radius (use average of X/Y scale for non-uniform scaling)
    float radiusPhysical = _radius * sm_uniformScale;

    float angleStep = XM_2PI / static_cast<float>(_segments);

    for (int i = 0; i < _segments; ++i)
    {
      if (sm_triangleVertices.size() + 3 > MAX_PRIMITIVE_VERTICES)
      {
        FlushPrimitives();
      }

      float angle1 = static_cast<float>(i) * angleStep;
      float angle2 = static_cast<float>(i + 1) * angleStep;

      float x1 = center.x + cosf(angle1) * radiusPhysical;
      float y1 = center.y + sinf(angle1) * radiusPhysical;
      float x2 = center.x + cosf(angle2) * radiusPhysical;
      float y2 = center.y + sinf(angle2) * radiusPhysical;

      sm_triangleVertices.emplace_back(XMFLOAT3{center.x, center.y, z}, colorF);
      sm_triangleVertices.emplace_back(XMFLOAT3{x1, y1, z}, colorF);
      sm_triangleVertices.emplace_back(XMFLOAT3{x2, y2, z}, colorF);
    }
  }

  void XM_CALLCONV Canvas::DrawCircleOutline(float _cx, float _cy, float _radius, float _thickness,
                                              FXMVECTOR _color, int _segments)
  {
    if (_segments < 3) _segments = 3;

    // Transform center to physical
    XMFLOAT2 center = LogicalToPhysical(_cx, _cy);
    // Scale radius and thickness
    float radiusPhysical = _radius * sm_uniformScale;
    float thicknessPhysical = _thickness * sm_uniformScale;

    float innerRadius = radiusPhysical - thicknessPhysical * 0.5f;
    float outerRadius = radiusPhysical + thicknessPhysical * 0.5f;

    XMFLOAT4 colorF;
    XMStoreFloat4(&colorF, _color);
    float z = sm_currentZ;

    float angleStep = XM_2PI / static_cast<float>(_segments);

    for (int i = 0; i < _segments; ++i)
    {
      if (sm_triangleVertices.size() + 6 > MAX_PRIMITIVE_VERTICES)
      {
        FlushPrimitives();
      }

      float angle1 = static_cast<float>(i) * angleStep;
      float angle2 = static_cast<float>(i + 1) * angleStep;

      float cos1 = cosf(angle1), sin1 = sinf(angle1);
      float cos2 = cosf(angle2), sin2 = sinf(angle2);

      float innerX1 = center.x + cos1 * innerRadius;
      float innerY1 = center.y + sin1 * innerRadius;
      float outerX1 = center.x + cos1 * outerRadius;
      float outerY1 = center.y + sin1 * outerRadius;
      float innerX2 = center.x + cos2 * innerRadius;
      float innerY2 = center.y + sin2 * innerRadius;
      float outerX2 = center.x + cos2 * outerRadius;
      float outerY2 = center.y + sin2 * outerRadius;

      // Two triangles per segment
      sm_triangleVertices.emplace_back(XMFLOAT3{innerX1, innerY1, z}, colorF);
      sm_triangleVertices.emplace_back(XMFLOAT3{outerX1, outerY1, z}, colorF);
      sm_triangleVertices.emplace_back(XMFLOAT3{innerX2, innerY2, z}, colorF);

      sm_triangleVertices.emplace_back(XMFLOAT3{outerX1, outerY1, z}, colorF);
      sm_triangleVertices.emplace_back(XMFLOAT3{outerX2, outerY2, z}, colorF);
      sm_triangleVertices.emplace_back(XMFLOAT3{innerX2, innerY2, z}, colorF);
    }
  }

  //=============================================================================
  // Sprite Rendering
  //=============================================================================

  int Canvas::LoadTexture(const std::wstring& _filename)
  {
    // Use TextureManager for proper loading, upload, and caching
    Texture* texture = TextureManager::LoadFromFile(_filename);

    if (!texture || !texture->IsValid())
    {
      DebugTrace(L"Canvas::LoadTexture - Failed to load: {}\n", _filename);
      return -1;
    }

    // Check if texture already registered (avoid duplicates)
    for (int i = 0; i < static_cast<int>(sm_textures.size()); ++i)
    {
      if (sm_textures[i] == texture)
      {
        return i;
      }
    }

    int index = static_cast<int>(sm_textures.size());
    sm_textures.push_back(texture);

    DebugTrace(L"Canvas::LoadTexture - Loaded: {} as index {}\n", _filename, index);
    return index;
  }

  void XM_CALLCONV Canvas::DrawSprite(int _textureIndex, float _x, float _y, float _width, float _height, FXMVECTOR _tint)
  {
    DrawSpriteUV(_textureIndex, _x, _y, _width, _height, 0.0f, 0.0f, 1.0f, 1.0f, _tint);
  }

  void XM_CALLCONV Canvas::DrawSpriteUV(int _textureIndex, float _x, float _y, float _width, float _height,
                                         float _u0, float _v0, float _u1, float _v1, FXMVECTOR _tint)
  {
    if (_textureIndex < 0 || _textureIndex >= static_cast<int>(sm_textures.size()))
    {
      return;
    }

    // Check if we need to flush due to texture change
    if (sm_currentTextureIndex != _textureIndex && sm_currentTextureIndex >= 0)
    {
      FlushSprites();
    }
    sm_currentTextureIndex = _textureIndex;

    RecordSpriteQuad(_x, _y, _width, _height, _u0, _v0, _u1, _v1, _tint);
  }

  void XM_CALLCONV Canvas::DrawSpriteRotated(int _textureIndex, float _cx, float _cy, float _width, float _height,
                                              float _rotation, FXMVECTOR _tint)
  {
    if (_textureIndex < 0 || _textureIndex >= static_cast<int>(sm_textures.size()))
    {
      return;
    }

    if (sm_currentTextureIndex != _textureIndex && sm_currentTextureIndex >= 0)
    {
      FlushSprites();
    }
    sm_currentTextureIndex = _textureIndex;

    if (sm_spriteVertices.size() + 6 > MAX_SPRITE_VERTICES)
    {
      FlushSprites();
    }

    XMFLOAT4 colorF;
    XMStoreFloat4(&colorF, _tint);
    float z = sm_currentZ;

    // Transform center to physical and scale dimensions
    XMFLOAT2 center = LogicalToPhysical(_cx, _cy);
    float hw = (_width * sm_uniformScale) * 0.5f;
    float hh = (_height * sm_uniformScale) * 0.5f;
    float cosR = cosf(_rotation);
    float sinR = sinf(_rotation);

    // Compute rotated corners in physical space
    auto rotatePoint = [&](float lx, float ly) -> XMFLOAT3 {
      float rx = lx * cosR - ly * sinR + center.x;
      float ry = lx * sinR + ly * cosR + center.y;
      return {rx, ry, z};
    };

    XMFLOAT3 tl = rotatePoint(-hw, -hh);
    XMFLOAT3 tr = rotatePoint(hw, -hh);
    XMFLOAT3 bl = rotatePoint(-hw, hh);
    XMFLOAT3 br = rotatePoint(hw, hh);

    // Two triangles
    sm_spriteVertices.emplace_back(tl, XMFLOAT2{0.0f, 0.0f}, colorF);
    sm_spriteVertices.emplace_back(tr, XMFLOAT2{1.0f, 0.0f}, colorF);
    sm_spriteVertices.emplace_back(bl, XMFLOAT2{0.0f, 1.0f}, colorF);

    sm_spriteVertices.emplace_back(tr, XMFLOAT2{1.0f, 0.0f}, colorF);
    sm_spriteVertices.emplace_back(br, XMFLOAT2{1.0f, 1.0f}, colorF);
    sm_spriteVertices.emplace_back(bl, XMFLOAT2{0.0f, 1.0f}, colorF);
  }

  void Canvas::RecordSpriteQuad(float _x, float _y, float _width, float _height,
                                 float _u0, float _v0, float _u1, float _v1, FXMVECTOR _color)
  {
    if (sm_spriteVertices.size() + 6 > MAX_SPRITE_VERTICES)
    {
      FlushSprites();
    }

    XMFLOAT4 colorF;
    XMStoreFloat4(&colorF, _color);
    float z = sm_currentZ;

    // Transform logical to physical coordinates
    XMFLOAT2 tl = LogicalToPhysical(_x, _y);
    XMFLOAT2 br = LogicalToPhysical(_x + _width, _y + _height);

    // Two triangles
    sm_spriteVertices.emplace_back(XMFLOAT3{tl.x, tl.y, z}, XMFLOAT2{_u0, _v0}, colorF);
    sm_spriteVertices.emplace_back(XMFLOAT3{br.x, tl.y, z}, XMFLOAT2{_u1, _v0}, colorF);
    sm_spriteVertices.emplace_back(XMFLOAT3{tl.x, br.y, z}, XMFLOAT2{_u0, _v1}, colorF);

    sm_spriteVertices.emplace_back(XMFLOAT3{br.x, tl.y, z}, XMFLOAT2{_u1, _v0}, colorF);
    sm_spriteVertices.emplace_back(XMFLOAT3{br.x, br.y, z}, XMFLOAT2{_u1, _v1}, colorF);
    sm_spriteVertices.emplace_back(XMFLOAT3{tl.x, br.y, z}, XMFLOAT2{_u0, _v1}, colorF);
  }

  Texture* Canvas::GetTexture(int _index)
  {
    if (_index < 0 || _index >= static_cast<int>(sm_textures.size()))
    {
      return nullptr;
    }
    return sm_textures[_index];
  }

  //=============================================================================
  // Text Rendering
  //=============================================================================

  FontId Canvas::LoadFont(const std::wstring& _fontTexture, 
                          uint32_t _glyphWidth, uint32_t _glyphHeight,
                          uint32_t _charsPerRow, uint32_t _firstChar)
  {
    int textureIndex = LoadTexture(_fontTexture);
    if (textureIndex < 0)
    {
      return FONT_INVALID;
    }

    auto* tex = GetTexture(textureIndex);
    if (!tex)
    {
      DebugTrace(L"Canvas::LoadFont - Failed to get texture: {}\n", _fontTexture);
      return FONT_INVALID;
    }

    auto font = std::make_unique<FontPrint>();

    if (font->LoadFromGrid(textureIndex, tex->GetWidth(), tex->GetHeight(),
                           _glyphWidth, _glyphHeight, _charsPerRow, _firstChar))
    {
      FontId id = static_cast<FontId>(sm_fonts.size());
      sm_fonts.push_back(std::move(font));
      DebugTrace(L"Canvas::LoadFont - Loaded font: {} as id {}\n", _fontTexture, id);
      return id;
    }

    DebugTrace(L"Canvas::LoadFont - Failed to load font: {}\n", _fontTexture);
    return FONT_INVALID;
  }

  void XM_CALLCONV Canvas::DrawText(FontId _fontId, float _x, float _y, const char* _text, FXMVECTOR _color, float _scale)
  {
    if (_fontId < 0 || _fontId >= static_cast<FontId>(sm_fonts.size()))
    {
      return;
    }

    auto& font = sm_fonts[_fontId];
    if (!font || !font->IsValid())
    {
      return;
    }

    // Flush if texture changes
    int fontTexture = font->GetTextureIndex();
    if (sm_currentTextureIndex != fontTexture && sm_currentTextureIndex >= 0)
    {
      FlushSprites();
    }
    sm_currentTextureIndex = fontTexture;

    // Record the starting index to transform vertices after emission
    size_t startIndex = sm_spriteVertices.size();

    // Emit glyphs in logical coordinates (FontPrint doesn't know about physical space)
    font->EmitGlyphs(sm_spriteVertices, _x, _y, sm_currentZ, _text, _color, _scale);

    // Transform all newly emitted vertices from logical to physical coordinates
    for (size_t i = startIndex; i < sm_spriteVertices.size(); ++i)
    {
      auto& v = sm_spriteVertices[i];
      XMFLOAT2 physical = LogicalToPhysical(v.m_position.x, v.m_position.y);
      v.m_position.x = physical.x;
      v.m_position.y = physical.y;
    }
  }

  void XM_CALLCONV Canvas::DrawTextCentered(FontId _fontId, float _centerX, float _y, const char* _text,
                                             FXMVECTOR _color, float _scale)
  {
    float width = MeasureTextWidth(_fontId, _text, _scale);
    DrawText(_fontId, _centerX - width * 0.5f, _y, _text, _color, _scale);
  }

  void XM_CALLCONV Canvas::DrawTextRight(FontId _fontId, float _rightX, float _y, const char* _text,
                                          FXMVECTOR _color, float _scale)
  {
    float width = MeasureTextWidth(_fontId, _text, _scale);
    DrawText(_fontId, _rightX - width, _y, _text, _color, _scale);
  }

  float Canvas::MeasureTextWidth(FontId _fontId, const char* _text, float _scale)
  {
    if (_fontId < 0 || _fontId >= static_cast<FontId>(sm_fonts.size()))
    {
      return 0.0f;
    }

    auto& font = sm_fonts[_fontId];
    if (!font || !font->IsValid())
    {
      return 0.0f;
    }

    return font->MeasureWidth(_text, _scale);
  }

  float Canvas::GetFontHeight(FontId _fontId, float _scale)
  {
    if (_fontId < 0 || _fontId >= static_cast<FontId>(sm_fonts.size()))
    {
      return 0.0f;
    }

    auto& font = sm_fonts[_fontId];
    if (!font || !font->IsValid())
    {
      return 0.0f;
    }

    return font->GetLineHeight(_scale);
  }

  //=============================================================================
  // Widget Helpers
  //=============================================================================

  void XM_CALLCONV Canvas::DrawPanel(const RECT& _rect, FXMVECTOR _color)
  {
    DrawRectangle(static_cast<float>(_rect.left), static_cast<float>(_rect.top),
                  static_cast<float>(_rect.right), static_cast<float>(_rect.bottom), _color);
  }

  void XM_CALLCONV Canvas::DrawPanelBordered(const RECT& _rect, FXMVECTOR _fillColor,
                                              FXMVECTOR _borderColor, float _borderThickness)
  {
    DrawPanel(_rect, _fillColor);
    DrawBorder(_rect, _borderThickness, _borderColor);
  }

  void XM_CALLCONV Canvas::DrawImage(const RECT& _rect, int _textureIndex, FXMVECTOR _tint)
  {
    float x = static_cast<float>(_rect.left);
    float y = static_cast<float>(_rect.top);
    float w = static_cast<float>(_rect.right - _rect.left);
    float h = static_cast<float>(_rect.bottom - _rect.top);
    DrawSprite(_textureIndex, x, y, w, h, _tint);
  }

  void XM_CALLCONV Canvas::DrawBorder(const RECT& _rect, float _thickness, FXMVECTOR _color)
  {
    DrawRectangleOutline(static_cast<float>(_rect.left), static_cast<float>(_rect.top),
                         static_cast<float>(_rect.right), static_cast<float>(_rect.bottom),
                         _thickness, _color);
  }

  //=============================================================================
  // Flush Operations
  //=============================================================================

  void Canvas::FlushPrimitives()
  {
    if (!Core::IsCommandListOpen())
    {
      return;
    }

    auto cmdList = Core::GetCommandList();

    // Ensure we're rendering to the backbuffer
    auto rtvHandle = Core::GetRenderTargetView();
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Set viewport for full physical backbuffer
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(sm_physicalWidth);
    viewport.Height = static_cast<float>(sm_physicalHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    cmdList->RSSetViewports(1, &viewport);

    // Apply current clip rect
    ApplyClipRect(sm_currentClipRect);

    // Flush lines
    if (!sm_lineVertices.empty())
    {
      size_t vertexDataSize = sm_lineVertices.size() * sizeof(VertexPositionColor);

      // Allocate from frame-isolated ring buffer
      auto alloc = FrameUploadAllocator::Allocate(vertexDataSize, alignof(VertexPositionColor));
      memcpy(alloc.cpuAddress, sm_lineVertices.data(), vertexDataSize);

      cmdList->SetGraphicsRootSignature(sm_primitiveRootSig.GetSignature());
      cmdList->SetPipelineState(sm_linePSO.GetPipelineStateObject());
      cmdList->SetGraphicsRootConstantBufferView(0, GetConstantBufferGPUAddress());
      cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

      D3D12_VERTEX_BUFFER_VIEW vbv;
      vbv.BufferLocation = alloc.gpuAddress;
      vbv.SizeInBytes = static_cast<UINT>(vertexDataSize);
      vbv.StrideInBytes = sizeof(VertexPositionColor);
      cmdList->IASetVertexBuffers(0, 1, &vbv);

      cmdList->DrawInstanced(static_cast<UINT>(sm_lineVertices.size()), 1, 0, 0);

      sm_lineVertices.clear();
    }

    // Flush triangles
    if (!sm_triangleVertices.empty())
    {
      size_t vertexDataSize = sm_triangleVertices.size() * sizeof(VertexPositionColor);

      // Allocate from frame-isolated ring buffer
      auto alloc = FrameUploadAllocator::Allocate(vertexDataSize, alignof(VertexPositionColor));
      memcpy(alloc.cpuAddress, sm_triangleVertices.data(), vertexDataSize);

      cmdList->SetGraphicsRootSignature(sm_primitiveRootSig.GetSignature());
      cmdList->SetPipelineState(sm_trianglePSO.GetPipelineStateObject());
      cmdList->SetGraphicsRootConstantBufferView(0, GetConstantBufferGPUAddress());
      cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      D3D12_VERTEX_BUFFER_VIEW vbv;
      vbv.BufferLocation = alloc.gpuAddress;
      vbv.SizeInBytes = static_cast<UINT>(vertexDataSize);
      vbv.StrideInBytes = sizeof(VertexPositionColor);
      cmdList->IASetVertexBuffers(0, 1, &vbv);

      cmdList->DrawInstanced(static_cast<UINT>(sm_triangleVertices.size()), 1, 0, 0);

      sm_triangleVertices.clear();
    }
  }

  void Canvas::FlushSprites()
  {
    if (!Core::IsCommandListOpen() || sm_spriteVertices.empty())
    {
      return;
    }

    if (sm_currentTextureIndex < 0 || sm_currentTextureIndex >= static_cast<int>(sm_textures.size()))
    {
      sm_spriteVertices.clear();
      return;
    }

    auto cmdList = Core::GetCommandList();
    auto* texture = sm_textures[sm_currentTextureIndex];

    if (!texture || !texture->IsValid())
    {
      sm_spriteVertices.clear();
      return;
    }

    // Ensure we're rendering to the backbuffer
    auto rtvHandle = Core::GetRenderTargetView();
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Set viewport for full physical backbuffer
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(sm_physicalWidth);
    viewport.Height = static_cast<float>(sm_physicalHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    cmdList->RSSetViewports(1, &viewport);

    // Apply current clip rect
    ApplyClipRect(sm_currentClipRect);

    size_t vertexDataSize = sm_spriteVertices.size() * sizeof(VertexPositionTextureColor);

    // Allocate from frame-isolated ring buffer
    auto alloc = FrameUploadAllocator::Allocate(vertexDataSize, alignof(VertexPositionTextureColor));
    memcpy(alloc.cpuAddress, sm_spriteVertices.data(), vertexDataSize);

    cmdList->SetGraphicsRootSignature(sm_spriteRootSig.GetSignature());
    cmdList->SetPipelineState(sm_spritePSO.GetPipelineStateObject());
    cmdList->SetGraphicsRootConstantBufferView(0, GetConstantBufferGPUAddress());
    cmdList->SetGraphicsRootDescriptorTable(1, texture->GetSRV());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VERTEX_BUFFER_VIEW vbv;
    vbv.BufferLocation = alloc.gpuAddress;
    vbv.SizeInBytes = static_cast<UINT>(vertexDataSize);
    vbv.StrideInBytes = sizeof(VertexPositionTextureColor);
    cmdList->IASetVertexBuffers(0, 1, &vbv);

    cmdList->DrawInstanced(static_cast<UINT>(sm_spriteVertices.size()), 1, 0, 0);

    sm_spriteVertices.clear();
  }

} // namespace Neuron::Graphics
