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
    CreateRenderTarget();
    CreateRootSignature();
    CreatePipelineStates();
    CreateDynamicBuffers();

    // Initialize default clip rect to full canvas
    sm_currentClipRect = {0, 0, static_cast<LONG>(CANVAS_WIDTH), static_cast<LONG>(CANVAS_HEIGHT)};

    UpdateProjection();

    DebugTrace("Canvas initialized: {}x{} render target\n", CANVAS_WIDTH, CANVAS_HEIGHT);
  }

  void Canvas::Shutdown()
  {
    DebugTrace("Canvas::Shutdown\n");

    Core::WaitForGpu();

    // Unmap all persistently mapped buffers
    if (sm_lineUploadBuffer && sm_lineUploadBufferMapped)
    {
      sm_lineUploadBuffer->Unmap(0, nullptr);
      sm_lineUploadBufferMapped = nullptr;
    }
    if (sm_triangleUploadBuffer && sm_triangleUploadBufferMapped)
    {
      sm_triangleUploadBuffer->Unmap(0, nullptr);
      sm_triangleUploadBufferMapped = nullptr;
    }
    if (sm_spriteUploadBuffer && sm_spriteUploadBufferMapped)
    {
      sm_spriteUploadBuffer->Unmap(0, nullptr);
      sm_spriteUploadBufferMapped = nullptr;
    }
    if (sm_constantBuffer && sm_constantBufferMapped)
    {
      sm_constantBuffer->Unmap(0, nullptr);
      sm_constantBufferMapped = nullptr;
    }

    // Release GPU resources
    sm_renderTarget = nullptr;
    sm_lineUploadBuffer = nullptr;
    sm_triangleUploadBuffer = nullptr;
    sm_spriteUploadBuffer = nullptr;
    sm_constantBuffer = nullptr;

    // Clear containers
    sm_lineVertices.clear();
    sm_triangleVertices.clear();
    sm_spriteVertices.clear();
    sm_clipStack.clear();
    sm_layerStack.clear();
    sm_textures.clear();
    sm_fonts.clear();
  }

  //=============================================================================
  // GPU Resource Creation
  //=============================================================================

  void Canvas::CreateRenderTarget()
  {
    auto device = Core::GetD3DDevice();

    // Create render target texture (1920x1080, BGRA with alpha for transparency)
    D3D12_RESOURCE_DESC rtDesc = {};
    rtDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rtDesc.Width = CANVAS_WIDTH;
    rtDesc.Height = CANVAS_HEIGHT;
    rtDesc.DepthOrArraySize = 1;
    rtDesc.MipLevels = 1;
    rtDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    rtDesc.SampleDesc.Count = 1;
    rtDesc.SampleDesc.Quality = 0;
    rtDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    rtDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = rtDesc.Format;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 0.0f;  // Fully transparent

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    check_hresult(device->CreateCommittedResource(
      &heapProps,
      D3D12_HEAP_FLAG_NONE,
      &rtDesc,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
      &clearValue,
      IID_PPV_ARGS(sm_renderTarget.put())));

    sm_renderTarget->SetName(L"Canvas Render Target");
    sm_renderTargetState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    // Create RTV
    auto rtvHandle = Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, true);
    sm_renderTargetRTV = rtvHandle;

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = rtDesc.Format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    device->CreateRenderTargetView(sm_renderTarget.get(), &rtvDesc, sm_renderTargetRTV);

    // Create SRV
    sm_renderTargetSRVHandle = Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
    sm_renderTargetSRV = sm_renderTargetSRVHandle;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = rtDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    device->CreateShaderResourceView(sm_renderTarget.get(), &srvDesc, sm_renderTargetSRVHandle);

    DebugTrace("Canvas render target created: {}x{}\n", CANVAS_WIDTH, CANVAS_HEIGHT);
  }

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

    // Line vertex buffer
    size_t lineBufferSize = MAX_PRIMITIVE_VERTICES * sizeof(VertexPositionColor);
    auto lineBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(lineBufferSize);
    check_hresult(device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE,
      &lineBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(sm_lineUploadBuffer.put())));
    sm_lineUploadBuffer->SetName(L"Canvas Line Upload Buffer");

    // Triangle vertex buffer
    size_t triangleBufferSize = MAX_PRIMITIVE_VERTICES * sizeof(VertexPositionColor);
    auto triangleBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(triangleBufferSize);
    check_hresult(device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE,
      &triangleBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(sm_triangleUploadBuffer.put())));
    sm_triangleUploadBuffer->SetName(L"Canvas Triangle Upload Buffer");

    // Sprite vertex buffer
    size_t spriteBufferSize = MAX_SPRITE_VERTICES * sizeof(VertexPositionTextureColor);
    auto spriteBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(spriteBufferSize);
    check_hresult(device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE,
      &spriteBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(sm_spriteUploadBuffer.put())));
    sm_spriteUploadBuffer->SetName(L"Canvas Sprite Upload Buffer");

    // Constant buffer (256-byte aligned)
    size_t constantBufferSize = (sizeof(CanvasConstants) + 255) & ~255;
    auto constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
    check_hresult(device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE,
      &constantBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(sm_constantBuffer.put())));
    sm_constantBuffer->SetName(L"Canvas Constant Buffer");

    // Persistently map all upload buffers (upload heaps can stay mapped for their lifetime)
    check_hresult(sm_lineUploadBuffer->Map(0, nullptr, &sm_lineUploadBufferMapped));
    check_hresult(sm_triangleUploadBuffer->Map(0, nullptr, &sm_triangleUploadBufferMapped));
    check_hresult(sm_spriteUploadBuffer->Map(0, nullptr, &sm_spriteUploadBufferMapped));
    check_hresult(sm_constantBuffer->Map(0, nullptr, &sm_constantBufferMapped));
  }

  void Canvas::UpdateProjection()
  {
    // Orthographic projection for fixed canvas size
    XMMATRIX proj = XMMatrixOrthographicOffCenterLH(
      0.0f, static_cast<float>(CANVAS_WIDTH),     // left, right
      static_cast<float>(CANVAS_HEIGHT), 0.0f,    // bottom, top (flipped for top-left origin)
      0.0f, 1.0f                                   // near, far
    );

    XMStoreFloat4x4(&sm_constants.Projection, XMMatrixTranspose(proj));

    if (sm_constantBufferMapped)
    {
      memcpy(sm_constantBufferMapped, &sm_constants, sizeof(CanvasConstants));
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

    // Reset clip rect to full canvas
    sm_currentClipRect = {0, 0, static_cast<LONG>(CANVAS_WIDTH), static_cast<LONG>(CANVAS_HEIGHT)};

    auto cmdList = Core::GetCommandList();

    // Transition render target to render target state
    if (sm_renderTargetState != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
      D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        sm_renderTarget.get(),
        sm_renderTargetState,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
      cmdList->ResourceBarrier(1, &barrier);
      sm_renderTargetState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    // Set render target and clear to transparent
    cmdList->OMSetRenderTargets(1, &sm_renderTargetRTV, FALSE, nullptr);

    constexpr float clearColor[] = {0.0f, 0.0f, 0.0f, 0.0f};  // Transparent
    cmdList->ClearRenderTargetView(sm_renderTargetRTV, clearColor, 0, nullptr);

    // Set viewport and scissor for canvas size
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(CANVAS_WIDTH);
    viewport.Height = static_cast<float>(CANVAS_HEIGHT);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    cmdList->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {0, 0, static_cast<LONG>(CANVAS_WIDTH), static_cast<LONG>(CANVAS_HEIGHT)};
    cmdList->RSSetScissorRects(1, &scissor);
  }

  void Canvas::Render()
  {
    // Flush all batched primitives to GPU
    FlushPrimitives();
    FlushSprites();

    auto cmdList = Core::GetCommandList();

    // Transition render target back to shader resource for sampling
    if (sm_renderTargetState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
    {
      D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        sm_renderTarget.get(),
        sm_renderTargetState,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      cmdList->ResourceBarrier(1, &barrier);
      sm_renderTargetState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
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
    sm_currentClipRect = {0, 0, static_cast<LONG>(CANVAS_WIDTH), static_cast<LONG>(CANVAS_HEIGHT)};
  }

  void Canvas::ApplyClipRect(const RECT& _rect)
  {
    auto cmdList = Core::GetCommandList();

    // Canvas uses fixed size, no scaling needed
    D3D12_RECT scissor;
    scissor.left = _rect.left;
    scissor.top = _rect.top;
    scissor.right = _rect.right;
    scissor.bottom = _rect.bottom;

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

    sm_lineVertices.emplace_back(XMFLOAT3{_x1, _y1, sm_currentZ}, colorF);
    sm_lineVertices.emplace_back(XMFLOAT3{_x2, _y2, sm_currentZ}, colorF);
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

    // Two triangles for the quad
    sm_triangleVertices.emplace_back(XMFLOAT3{_left, _top, z}, colorF);
    sm_triangleVertices.emplace_back(XMFLOAT3{_right, _top, z}, colorF);
    sm_triangleVertices.emplace_back(XMFLOAT3{_left, _bottom, z}, colorF);

    sm_triangleVertices.emplace_back(XMFLOAT3{_right, _top, z}, colorF);
    sm_triangleVertices.emplace_back(XMFLOAT3{_right, _bottom, z}, colorF);
    sm_triangleVertices.emplace_back(XMFLOAT3{_left, _bottom, z}, colorF);
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

    sm_triangleVertices.emplace_back(XMFLOAT3{_x1, _y1, z}, colorF);
    sm_triangleVertices.emplace_back(XMFLOAT3{_x2, _y2, z}, colorF);
    sm_triangleVertices.emplace_back(XMFLOAT3{_x3, _y3, z}, colorF);
  }

  void XM_CALLCONV Canvas::DrawCircle(float _cx, float _cy, float _radius, FXMVECTOR _color, int _segments)
  {
    if (_segments < 3) _segments = 3;

    XMFLOAT4 colorF;
    XMStoreFloat4(&colorF, _color);
    float z = sm_currentZ;

    float angleStep = XM_2PI / static_cast<float>(_segments);

    for (int i = 0; i < _segments; ++i)
    {
      if (sm_triangleVertices.size() + 3 > MAX_PRIMITIVE_VERTICES)
      {
        FlushPrimitives();
      }

      float angle1 = static_cast<float>(i) * angleStep;
      float angle2 = static_cast<float>(i + 1) * angleStep;

      float x1 = _cx + cosf(angle1) * _radius;
      float y1 = _cy + sinf(angle1) * _radius;
      float x2 = _cx + cosf(angle2) * _radius;
      float y2 = _cy + sinf(angle2) * _radius;

      sm_triangleVertices.emplace_back(XMFLOAT3{_cx, _cy, z}, colorF);
      sm_triangleVertices.emplace_back(XMFLOAT3{x1, y1, z}, colorF);
      sm_triangleVertices.emplace_back(XMFLOAT3{x2, y2, z}, colorF);
    }
  }

  void XM_CALLCONV Canvas::DrawCircleOutline(float _cx, float _cy, float _radius, float _thickness,
                                              FXMVECTOR _color, int _segments)
  {
    if (_segments < 3) _segments = 3;

    float innerRadius = _radius - _thickness * 0.5f;
    float outerRadius = _radius + _thickness * 0.5f;

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

      float innerX1 = _cx + cos1 * innerRadius;
      float innerY1 = _cy + sin1 * innerRadius;
      float outerX1 = _cx + cos1 * outerRadius;
      float outerY1 = _cy + sin1 * outerRadius;
      float innerX2 = _cx + cos2 * innerRadius;
      float innerY2 = _cy + sin2 * innerRadius;
      float outerX2 = _cx + cos2 * outerRadius;
      float outerY2 = _cy + sin2 * outerRadius;

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

    float hw = _width * 0.5f;
    float hh = _height * 0.5f;
    float cosR = cosf(_rotation);
    float sinR = sinf(_rotation);

    // Compute rotated corners
    auto rotatePoint = [&](float lx, float ly) -> XMFLOAT3 {
      float rx = lx * cosR - ly * sinR + _cx;
      float ry = lx * sinR + ly * cosR + _cy;
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

    float x0 = _x;
    float y0 = _y;
    float x1 = _x + _width;
    float y1 = _y + _height;

    // Two triangles
    sm_spriteVertices.emplace_back(XMFLOAT3{x0, y0, z}, XMFLOAT2{_u0, _v0}, colorF);
    sm_spriteVertices.emplace_back(XMFLOAT3{x1, y0, z}, XMFLOAT2{_u1, _v0}, colorF);
    sm_spriteVertices.emplace_back(XMFLOAT3{x0, y1, z}, XMFLOAT2{_u0, _v1}, colorF);

    sm_spriteVertices.emplace_back(XMFLOAT3{x1, y0, z}, XMFLOAT2{_u1, _v0}, colorF);
    sm_spriteVertices.emplace_back(XMFLOAT3{x1, y1, z}, XMFLOAT2{_u1, _v1}, colorF);
    sm_spriteVertices.emplace_back(XMFLOAT3{x0, y1, z}, XMFLOAT2{_u0, _v1}, colorF);
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

    font->EmitGlyphs(sm_spriteVertices, _x, _y, sm_currentZ, _text, _color, _scale);
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

    // Apply current clip rect
    ApplyClipRect(sm_currentClipRect);

    // Flush lines
    if (!sm_lineVertices.empty())
    {
      size_t vertexDataSize = sm_lineVertices.size() * sizeof(VertexPositionColor);

      // Use persistently mapped buffer
      memcpy(sm_lineUploadBufferMapped, sm_lineVertices.data(), vertexDataSize);

      cmdList->SetGraphicsRootSignature(sm_primitiveRootSig.GetSignature());
      cmdList->SetPipelineState(sm_linePSO.GetPipelineStateObject());
      cmdList->SetGraphicsRootConstantBufferView(0, sm_constantBuffer->GetGPUVirtualAddress());
      cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

      D3D12_VERTEX_BUFFER_VIEW vbv;
      vbv.BufferLocation = sm_lineUploadBuffer->GetGPUVirtualAddress();
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

      // Use persistently mapped buffer
      memcpy(sm_triangleUploadBufferMapped, sm_triangleVertices.data(), vertexDataSize);

      cmdList->SetGraphicsRootSignature(sm_primitiveRootSig.GetSignature());
      cmdList->SetPipelineState(sm_trianglePSO.GetPipelineStateObject());
      cmdList->SetGraphicsRootConstantBufferView(0, sm_constantBuffer->GetGPUVirtualAddress());
      cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      D3D12_VERTEX_BUFFER_VIEW vbv;
      vbv.BufferLocation = sm_triangleUploadBuffer->GetGPUVirtualAddress();
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

    // Apply current clip rect
    ApplyClipRect(sm_currentClipRect);

    size_t vertexDataSize = sm_spriteVertices.size() * sizeof(VertexPositionTextureColor);

    // Use persistently mapped buffer
    memcpy(sm_spriteUploadBufferMapped, sm_spriteVertices.data(), vertexDataSize);

    cmdList->SetGraphicsRootSignature(sm_spriteRootSig.GetSignature());
    cmdList->SetPipelineState(sm_spritePSO.GetPipelineStateObject());
    cmdList->SetGraphicsRootConstantBufferView(0, sm_constantBuffer->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(1, texture->GetSRV());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VERTEX_BUFFER_VIEW vbv;
    vbv.BufferLocation = sm_spriteUploadBuffer->GetGPUVirtualAddress();
    vbv.SizeInBytes = static_cast<UINT>(vertexDataSize);
    vbv.StrideInBytes = sizeof(VertexPositionTextureColor);
    cmdList->IASetVertexBuffers(0, 1, &vbv);

    cmdList->DrawInstanced(static_cast<UINT>(sm_spriteVertices.size()), 1, 0, 0);

    sm_spriteVertices.clear();
  }

} // namespace Neuron::Graphics
