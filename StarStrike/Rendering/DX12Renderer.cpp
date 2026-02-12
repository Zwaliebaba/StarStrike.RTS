#include "pch.h"
#include "DX12Renderer.h"
#include "GraphicsCore.h"
#include "SamplerManager.h"
#include "GdiBitmap.h"

// Include compiled shader bytecode (these will be generated from HLSL files)
#include "CompiledShaders/BasicVS.h"
#include "CompiledShaders/BasicPS.h"
#include "CompiledShaders/SpriteVS.h"
#include "CompiledShaders/SpritePS.h"

using namespace Neuron::Graphics;

namespace StarStrike
{
  void DX12Renderer::Startup()
  {
    DebugTrace("DX12Renderer::Startup\n");

    // Reserve space for batched vertices
    sm_lineVertices.reserve(MAX_BATCH_VERTICES);
    sm_triangleVertices.reserve(MAX_BATCH_VERTICES);

    // Create rendering resources
    CreateRootSignatures();
    CreatePipelineStates();
    CreateDynamicBuffers();

    // Get initial screen size
    RECT outputSize = Core::GetOutputSize();
    sm_screenWidth = static_cast<float>(outputSize.right - outputSize.left);
    sm_screenHeight = static_cast<float>(outputSize.bottom - outputSize.top);

    UpdateProjectionMatrix();

    DebugTrace("DX12Renderer initialized: {}x{}\n", sm_screenWidth, sm_screenHeight);
  }

  void DX12Renderer::Shutdown()
  {
    DebugTrace("DX12Renderer::Shutdown\n");

    // Wait for GPU to finish before releasing resources
    Core::WaitForGpu();

    // Unmap constant buffer
    if (sm_constantUploadBuffer && sm_constantBufferMapped)
    {
      sm_constantUploadBuffer->Unmap(0, nullptr);
      sm_constantBufferMapped = nullptr;
    }

    // Release constant buffer
    sm_constantUploadBuffer = nullptr;

    // Clear textures and legacy sprite map
    sm_textures.clear();
    sm_legacySpriteMap.clear();

    // Clear vertex data
    sm_lineVertices.clear();
    sm_triangleVertices.clear();
    sm_spriteVertices.clear();
    sm_sprites.clear();
  }

  void DX12Renderer::CreateRootSignatures()
  {
    // Basic root signature: one constant buffer for WorldViewProj matrix
    // Root Parameter 0: CBV at b0
    sm_basicRootSignature.Reset(1, 0);
    sm_basicRootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
    sm_basicRootSignature.Finalize(L"Basic Root Signature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

    // Sprite root signature: CBV + texture SRV + sampler
    sm_spriteRootSignature.Reset(2, 1);
    sm_spriteRootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
    sm_spriteRootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);

    // Static sampler for sprite rendering
    SamplerDesc spriteSampler;
    spriteSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    spriteSampler.SetTextureAddressMode(D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    sm_spriteRootSignature.InitStaticSampler(0, spriteSampler, D3D12_SHADER_VISIBILITY_PIXEL);

    sm_spriteRootSignature.Finalize(L"Sprite Root Signature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);
  }

  void DX12Renderer::CreatePipelineStates()
  {
    // Depth stencil state with depth disabled for 2D rendering
    CD3DX12_DEPTH_STENCIL_DESC noDepthDesc(D3D12_DEFAULT);
    noDepthDesc.DepthEnable = FALSE;
    noDepthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    // Line PSO - no depth testing for 2D lines
    sm_linePSO.SetRootSignature(sm_basicRootSignature);
    sm_linePSO.SetVertexShader(g_pBasicVS, sizeof(g_pBasicVS));
    sm_linePSO.SetPixelShader(g_pBasicPS, sizeof(g_pBasicPS));
    sm_linePSO.SetInputLayout(&VertexPositionColor::INPUT_LAYOUT);
    sm_linePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
    sm_linePSO.SetRenderTargetFormat(Core::GetBackBufferFormat(), DXGI_FORMAT_UNKNOWN);
    sm_linePSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
    sm_linePSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
    sm_linePSO.SetDepthStencilState(noDepthDesc);
    sm_linePSO.Finalize();

    // Triangle PSO - no depth testing for 2D triangles
    sm_trianglePSO.SetRootSignature(sm_basicRootSignature);
    sm_trianglePSO.SetVertexShader(g_pBasicVS, sizeof(g_pBasicVS));
    sm_trianglePSO.SetPixelShader(g_pBasicPS, sizeof(g_pBasicPS));
    sm_trianglePSO.SetInputLayout(&VertexPositionColor::INPUT_LAYOUT);
    sm_trianglePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    sm_trianglePSO.SetRenderTargetFormat(Core::GetBackBufferFormat(), DXGI_FORMAT_UNKNOWN);
    sm_trianglePSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
    sm_trianglePSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
    sm_trianglePSO.SetDepthStencilState(noDepthDesc);
    sm_trianglePSO.Finalize();

    // Sprite PSO - alpha blending for textured sprites
    CD3DX12_BLEND_DESC alphaBlendDesc(D3D12_DEFAULT);
    alphaBlendDesc.RenderTarget[0].BlendEnable = TRUE;
    alphaBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    alphaBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    alphaBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    alphaBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    alphaBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    alphaBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

    sm_spritePSO.SetRootSignature(sm_spriteRootSignature);
    sm_spritePSO.SetVertexShader(g_pSpriteVS, sizeof(g_pSpriteVS));
    sm_spritePSO.SetPixelShader(g_pSpritePS, sizeof(g_pSpritePS));
    sm_spritePSO.SetInputLayout(&VertexPositionTextureColor::INPUT_LAYOUT);
    sm_spritePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    sm_spritePSO.SetRenderTargetFormat(Core::GetBackBufferFormat(), DXGI_FORMAT_UNKNOWN);
    sm_spritePSO.SetBlendState(alphaBlendDesc);
    sm_spritePSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
    sm_spritePSO.SetDepthStencilState(noDepthDesc);
    sm_spritePSO.Finalize();
  }

  void DX12Renderer::CreateDynamicBuffers()
  {
    auto device = Core::GetD3DDevice();

    // Create upload heap properties
    auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // Per-frame constant buffer (3 frames in flight, 256-byte aligned each)
    static constexpr UINT MAX_FRAMES = 3;
    size_t constantBufferSize = CONSTANT_BUFFER_FRAME_SIZE * MAX_FRAMES;
    auto constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
    check_hresult(device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &constantBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(sm_constantUploadBuffer.put())));
    sm_constantUploadBuffer->SetName(L"DX12Renderer Constant Buffer (Per-Frame)");

    // Map constant buffer permanently
    check_hresult(sm_constantUploadBuffer->Map(0, nullptr, &sm_constantBufferMapped));

    // Reserve sprite vertex storage
    sm_spriteVertices.reserve(MAX_BATCH_VERTICES);
    sm_sprites.reserve(1024);
  }

  D3D12_GPU_VIRTUAL_ADDRESS DX12Renderer::GetConstantBufferGPUAddress()
  {
    UINT frameIndex = Core::GetCurrentFrameIndex();
    return sm_constantUploadBuffer->GetGPUVirtualAddress() + (CONSTANT_BUFFER_FRAME_SIZE * frameIndex);
  }

  void DX12Renderer::UpdateProjectionMatrix()
  {
    // Create orthographic projection for 2D rendering
    // The game uses an 800x600 virtual coordinate system
    // Map virtual coordinates to screen space
    constexpr float VIRTUAL_WIDTH = 800.0f;
    constexpr float VIRTUAL_HEIGHT = 600.0f;

    XMMATRIX proj = XMMatrixOrthographicOffCenterLH(0.0f, VIRTUAL_WIDTH,// left, right
                                                    VIRTUAL_HEIGHT, 0.0f,// bottom, top (flipped for top-left origin)
                                                    0.0f, 1.0f// near, far
        );

    XMStoreFloat4x4(&sm_constants.WorldViewProj, XMMatrixTranspose(proj));

    // Write to current frame's constant buffer slice
    if (sm_constantBufferMapped)
    {
      UINT frameIndex = Core::GetCurrentFrameIndex();
      auto* dest = static_cast<uint8_t*>(sm_constantBufferMapped) + (CONSTANT_BUFFER_FRAME_SIZE * frameIndex);
      memcpy(dest, &sm_constants, sizeof(BasicConstants));
    }
  }

  void DX12Renderer::BeginFrame()
  {
    // Clear batched vertices
    sm_lineVertices.clear();
    sm_triangleVertices.clear();
    sm_sprites.clear();
    sm_spriteVertices.clear();

    // Update projection if screen size changed
    RECT outputSize = Core::GetOutputSize();
    float newWidth = static_cast<float>(outputSize.right - outputSize.left);
    float newHeight = static_cast<float>(outputSize.bottom - outputSize.top);

    if (newWidth != sm_screenWidth || newHeight != sm_screenHeight)
    {
      sm_screenWidth = newWidth;
      sm_screenHeight = newHeight;
      UpdateProjectionMatrix();
    }

    auto cmdList = Core::GetCommandList();

    // Clear render target
    constexpr float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    cmdList->ClearRenderTargetView(Core::GetRenderTargetView(), clearColor, 0, nullptr);

    // Clear depth stencil for 3D rendering
    if (Core::GetDepthBufferFormat() != DXGI_FORMAT_UNKNOWN) cmdList->ClearDepthStencilView(Core::GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set render target and viewport
    auto rtvHandle = Core::GetRenderTargetView();
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    cmdList->RSSetViewports(1, &Core::GetScreenViewport());

    // Reset clip region to full screen
    ResetClipRegion();
    ApplyClipRegion();
  }

  void DX12Renderer::SetClipRegion(int left, int top, int right, int bottom)
  {
    // Flush pending primitives before changing clip region
    FlushLines();
    FlushTriangles();
    FlushSprites();

    sm_clipLeft = left;
    sm_clipTop = top;
    sm_clipRight = right;
    sm_clipBottom = bottom;
    sm_clipDirty = true;

    ApplyClipRegion();
  }

  void DX12Renderer::ResetClipRegion()
  {
    sm_clipLeft = 0;
    sm_clipTop = 0;
    sm_clipRight = 512;
    sm_clipBottom = 512;
    sm_clipDirty = true;
  }

  void DX12Renderer::ApplyClipRegion()
  {
    if (!sm_clipDirty) return;

    // Don't issue GPU commands if the command list is closed
    // The clip region will be applied when BeginFrame() is called
    if (!Core::IsCommandListOpen()) return;

    auto cmdList = Core::GetCommandList();

    // Get screen dimensions
    RECT outputSize = Core::GetOutputSize();
    float screenW = static_cast<float>(outputSize.right - outputSize.left);
    float screenH = static_cast<float>(outputSize.bottom - outputSize.top);

    // GFX_X_OFFSET and GFX_Y_OFFSET are defined in gfx.h (144 and 44)
    // The game uses an 800x600 virtual screen with the game area at offset
    constexpr float GFX_X_OFFSET = 144.0f;
    constexpr float GFX_Y_OFFSET = 44.0f;
    constexpr float VIRTUAL_WIDTH = 800.0f;
    constexpr float VIRTUAL_HEIGHT = 600.0f;

    // Convert game coordinates to screen coordinates
    float scaleX = screenW / VIRTUAL_WIDTH;
    float scaleY = screenH / VIRTUAL_HEIGHT;

    D3D12_RECT scissor;
    scissor.left = static_cast<LONG>((sm_clipLeft + GFX_X_OFFSET) * scaleX);
    scissor.top = static_cast<LONG>((sm_clipTop + GFX_Y_OFFSET) * scaleY);
    scissor.right = static_cast<LONG>((sm_clipRight + GFX_X_OFFSET) * scaleX);
    scissor.bottom = static_cast<LONG>((sm_clipBottom + GFX_Y_OFFSET) * scaleY);

    cmdList->RSSetScissorRects(1, &scissor);
    sm_clipDirty = false;
  }

  void DX12Renderer::EndFrame()
  {
    // Flush any remaining batched primitives
    FlushLines();
    FlushTriangles();
    FlushSprites();
  }

  void DX12Renderer::DrawPixel(float x, float y, const XMFLOAT4 &color)
  {
    // Draw pixel as a small rectangle (1x1)
    DrawRectangle(x, y, x + 1.0f, y + 1.0f, color);
  }

  void DX12Renderer::DrawLine(float x1, float y1, float x2, float y2, const XMFLOAT4 &color)
  {
    if (sm_lineVertices.size() + 2 > MAX_BATCH_VERTICES) FlushLines();

    sm_lineVertices.emplace_back(XMFLOAT3(x1, y1, 0.0f), color);
    sm_lineVertices.emplace_back(XMFLOAT3(x2, y2, 0.0f), color);
  }

  void DX12Renderer::DrawTriangle(float x1, float y1, float x2, float y2, float x3, float y3, const XMFLOAT4 &color)
  {
    if (sm_triangleVertices.size() + 3 > MAX_BATCH_VERTICES) FlushTriangles();

    sm_triangleVertices.emplace_back(XMFLOAT3(x1, y1, 0.0f), color);
    sm_triangleVertices.emplace_back(XMFLOAT3(x2, y2, 0.0f), color);
    sm_triangleVertices.emplace_back(XMFLOAT3(x3, y3, 0.0f), color);
  }

  void DX12Renderer::DrawRectangle(float left, float top, float right, float bottom, const XMFLOAT4 &color)
  {
    // Draw as two triangles
    DrawTriangle(left, top, right, top, right, bottom, color);
    DrawTriangle(left, top, right, bottom, left, bottom, color);
  }

  void DX12Renderer::DrawCircle(float cx, float cy, float radius, const XMFLOAT4 &color, bool filled)
  {
    constexpr int CIRCLE_SEGMENTS = 32;
    constexpr float PI = 3.14159265358979f;

    if (filled)
    {
      // Draw filled circle as triangle fan (center + segments)
      for (int i = 0; i < CIRCLE_SEGMENTS; i++)
      {
        float angle1 = (static_cast<float>(i) / CIRCLE_SEGMENTS) * 2.0f * PI;
        float angle2 = (static_cast<float>(i + 1) / CIRCLE_SEGMENTS) * 2.0f * PI;

        float x1 = cx + radius * cosf(angle1);
        float y1 = cy + radius * sinf(angle1);
        float x2 = cx + radius * cosf(angle2);
        float y2 = cy + radius * sinf(angle2);

        DrawTriangle(cx, cy, x1, y1, x2, y2, color);
      }
    }
    else
    {
      // Draw wireframe circle as line loop
      for (int i = 0; i < CIRCLE_SEGMENTS; i++)
      {
        float angle1 = (static_cast<float>(i) / CIRCLE_SEGMENTS) * 2.0f * PI;
        float angle2 = (static_cast<float>(i + 1) / CIRCLE_SEGMENTS) * 2.0f * PI;

        float x1 = cx + radius * cosf(angle1);
        float y1 = cy + radius * sinf(angle1);
        float x2 = cx + radius * cosf(angle2);
        float y2 = cy + radius * sinf(angle2);

        DrawLine(x1, y1, x2, y2, color);
      }
    }
  }

  void DX12Renderer::FlushLines()
  {
    if (sm_lineVertices.empty()) return;

    auto cmdList = Core::GetCommandList();

    // Set render target (without depth for 2D)
    auto rtvHandle = Core::GetRenderTargetView();
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    cmdList->RSSetViewports(1, &Core::GetScreenViewport());
    ApplyClipRegion();

    // Allocate from frame-isolated ring buffer
    size_t dataSize = sm_lineVertices.size() * sizeof(VertexPositionColor);
    auto alloc = FrameUploadAllocator::Allocate(dataSize, alignof(VertexPositionColor));
    memcpy(alloc.cpuAddress, sm_lineVertices.data(), dataSize);

    // Set pipeline state
    cmdList->SetPipelineState(sm_linePSO.GetPipelineStateObject());
    cmdList->SetGraphicsRootSignature(sm_basicRootSignature.GetSignature());
    cmdList->SetGraphicsRootConstantBufferView(0, GetConstantBufferGPUAddress());

    // Set vertex buffer
    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    vbv.BufferLocation = alloc.gpuAddress;
    vbv.SizeInBytes = static_cast<UINT>(dataSize);
    vbv.StrideInBytes = sizeof(VertexPositionColor);
    cmdList->IASetVertexBuffers(0, 1, &vbv);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    // Draw
    cmdList->DrawInstanced(static_cast<UINT>(sm_lineVertices.size()), 1, 0, 0);

    sm_lineVertices.clear();
  }

  void DX12Renderer::FlushTriangles()
  {
    if (sm_triangleVertices.empty()) return;

    auto cmdList = Core::GetCommandList();

    // Set render target (without depth for 2D)
    auto rtvHandle = Core::GetRenderTargetView();
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    cmdList->RSSetViewports(1, &Core::GetScreenViewport());
    ApplyClipRegion();

    // Allocate from frame-isolated ring buffer
    size_t dataSize = sm_triangleVertices.size() * sizeof(VertexPositionColor);
    auto alloc = FrameUploadAllocator::Allocate(dataSize, alignof(VertexPositionColor));
    memcpy(alloc.cpuAddress, sm_triangleVertices.data(), dataSize);

    // Set pipeline state
    cmdList->SetPipelineState(sm_trianglePSO.GetPipelineStateObject());
    cmdList->SetGraphicsRootSignature(sm_basicRootSignature.GetSignature());
    cmdList->SetGraphicsRootConstantBufferView(0, GetConstantBufferGPUAddress());

    // Set vertex buffer
    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    vbv.BufferLocation = alloc.gpuAddress;
    vbv.SizeInBytes = static_cast<UINT>(dataSize);
    vbv.StrideInBytes = sizeof(VertexPositionColor);
    cmdList->IASetVertexBuffers(0, 1, &vbv);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Draw
    cmdList->DrawInstanced(static_cast<UINT>(sm_triangleVertices.size()), 1, 0, 0);

    sm_triangleVertices.clear();
  }

  ID3D12GraphicsCommandList *DX12Renderer::GetCommandList() { return Core::GetCommandList(); }

  int DX12Renderer::LoadTexture(const std::wstring &filename)
  {
    auto texture = std::make_unique<Texture2D>();
    if (!texture->CreateFromFile(filename))
    {
      DebugTrace("DX12Renderer::LoadTexture - Failed to load: {}\n", std::string(filename.begin(), filename.end()));
      return -1;
    }

    int index = static_cast<int>(sm_textures.size());
    sm_textures.push_back(std::move(texture));
    DebugTrace("DX12Renderer::LoadTexture - Loaded texture {} at index {}\n", std::string(filename.begin(), filename.end()), index);
    return index;
  }

  int DX12Renderer::LoadTextureFromBitmap(const GdiBitmap *bitmap)
  {
    if (!bitmap) return -1;

    auto texture = std::make_unique<Texture2D>();
    if (!texture->CreateFromBitmap(bitmap))
    {
      DebugTrace("DX12Renderer::LoadTextureFromBitmap - Failed to create texture from bitmap\n");
      return -1;
    }

    int index = static_cast<int>(sm_textures.size());
    sm_textures.push_back(std::move(texture));
    return index;
  }

  void DX12Renderer::DrawSprite(int textureIndex, float x, float y, float width, float height, const XMFLOAT4 &color) { DrawSpriteUV(textureIndex, x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f, color); }

  void DX12Renderer::DrawSpriteUV(int textureIndex, float x, float y, float width, float height, float u0, float v0, float u1, float v1, const XMFLOAT4 &color)
  {
    if (textureIndex < 0 || textureIndex >= static_cast<int>(sm_textures.size())) return;

    SpriteInfo sprite;
    sprite.textureIndex = textureIndex;
    sprite.x = x;
    sprite.y = y;
    sprite.width = width;
    sprite.height = height;
    sprite.u0 = u0;
    sprite.v0 = v0;
    sprite.u1 = u1;
    sprite.v1 = v1;
    sprite.color = color;

    sm_sprites.push_back(sprite);
  }

  void DX12Renderer::FlushSprites()
  {
    if (sm_sprites.empty()) return;

    // Flush other primitives first to maintain draw order
    FlushLines();
    FlushTriangles();

    auto cmdList = Core::GetCommandList();

    // Ensure descriptor heaps are bound for shader-visible resources
    DescriptorAllocator::SetDescriptorHeaps(cmdList);

    // Sort sprites by texture to minimize state changes (optional optimization)
    // For now, just render in order

    int currentTexture = -1;

    for (const auto &sprite: sm_sprites)
    {
      // Build vertex data for this sprite (6 vertices = 2 triangles)
      float left = sprite.x;
      float top = sprite.y;
      float right = sprite.x + sprite.width;
      float bottom = sprite.y + sprite.height;

      VertexPositionTextureColor vertices[6] = {
          // Triangle 1
          {XMFLOAT3(left, top, 0.0f), XMFLOAT2(sprite.u0, sprite.v0), sprite.color}, {XMFLOAT3(right, top, 0.0f), XMFLOAT2(sprite.u1, sprite.v0), sprite.color}, {XMFLOAT3(right, bottom, 0.0f), XMFLOAT2(sprite.u1, sprite.v1), sprite.color},
          // Triangle 2
          {XMFLOAT3(left, top, 0.0f), XMFLOAT2(sprite.u0, sprite.v0), sprite.color}, {XMFLOAT3(right, bottom, 0.0f), XMFLOAT2(sprite.u1, sprite.v1), sprite.color}, {XMFLOAT3(left, bottom, 0.0f), XMFLOAT2(sprite.u0, sprite.v1), sprite.color},};

      // If texture changed, we need to set up new state
      if (sprite.textureIndex != currentTexture)
      {
        // Flush any pending vertices
        if (!sm_spriteVertices.empty())
        {
          // Allocate from frame-isolated ring buffer
          size_t dataSize = sm_spriteVertices.size() * sizeof(VertexPositionTextureColor);
          auto alloc = FrameUploadAllocator::Allocate(dataSize, alignof(VertexPositionTextureColor));
          memcpy(alloc.cpuAddress, sm_spriteVertices.data(), dataSize);

          D3D12_VERTEX_BUFFER_VIEW vbv = {};
          vbv.BufferLocation = alloc.gpuAddress;
          vbv.SizeInBytes = static_cast<UINT>(dataSize);
          vbv.StrideInBytes = sizeof(VertexPositionTextureColor);
          cmdList->IASetVertexBuffers(0, 1, &vbv);
          cmdList->DrawInstanced(static_cast<UINT>(sm_spriteVertices.size()), 1, 0, 0);
          sm_spriteVertices.clear();
        }

        currentTexture = sprite.textureIndex;

        // Set render target (without depth for 2D sprites)
        auto rtvHandle = Core::GetRenderTargetView();
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        cmdList->RSSetViewports(1, &Core::GetScreenViewport());
        ApplyClipRegion();

        // Set pipeline state for sprites
        cmdList->SetPipelineState(sm_spritePSO.GetPipelineStateObject());
        cmdList->SetGraphicsRootSignature(sm_spriteRootSignature.GetSignature());
        cmdList->SetGraphicsRootConstantBufferView(0, GetConstantBufferGPUAddress());

        // Set texture
        auto &tex = sm_textures[currentTexture];
        cmdList->SetGraphicsRootDescriptorTable(1, tex->GetGpuSRV());

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      }

      // Add vertices to batch
      for (int i = 0; i < 6; i++) sm_spriteVertices.push_back(vertices[i]);
    }

    // Flush remaining vertices
    if (!sm_spriteVertices.empty())
    {
      // Allocate from frame-isolated ring buffer
      size_t dataSize = sm_spriteVertices.size() * sizeof(VertexPositionTextureColor);
      auto alloc = FrameUploadAllocator::Allocate(dataSize, alignof(VertexPositionTextureColor));
      memcpy(alloc.cpuAddress, sm_spriteVertices.data(), dataSize);

      D3D12_VERTEX_BUFFER_VIEW vbv;
      vbv.BufferLocation = alloc.gpuAddress;
      vbv.SizeInBytes = static_cast<UINT>(dataSize);
      vbv.StrideInBytes = sizeof(VertexPositionTextureColor);
      cmdList->IASetVertexBuffers(0, 1, &vbv);

      cmdList->DrawInstanced(static_cast<UINT>(sm_spriteVertices.size()), 1, 0, 0);
      sm_spriteVertices.clear();
    }

    sm_sprites.clear();
  }

  void DX12Renderer::RegisterLegacySprite(int legacySpriteIndex, const std::wstring &filename, int srcX, int srcY, int size)
  {
    DebugTrace("DX12Renderer::RegisterLegacySprite - Registering sprite {} from {} at ({},{}) size {}\n", legacySpriteIndex, std::string(filename.begin(), filename.end()), srcX, srcY, size);

    // Load the full bitmap
    auto fname = FileSys::GetHomeDirectoryA() + std::string(filename.begin(), filename.end());
    DebugTrace("DX12Renderer::RegisterLegacySprite - Full path: {}\n", fname);

    auto bitmap = GdiBitmapLoader::LoadBMP(fname);

    if (!bitmap)
    {
      DebugTrace("DX12Renderer::RegisterLegacySprite - FAILED to load bitmap: {}\n", fname);
      return;
    }

    DebugTrace("DX12Renderer::RegisterLegacySprite - Bitmap loaded: {}x{}, bpp={}, pitch={}\n", bitmap->width, bitmap->height, bitmap->bitsPerPixel, bitmap->pitch);

    // For now, load the full texture (sub-region extraction can be added later)
    // The legacy code uses gfx_load_texture which extracts a region
    auto texture = std::make_unique<Texture2D>();

    // Extract the sub-region from the bitmap
    // If srcX=0, srcY=0 and size exceeds bitmap, load the full texture instead
    bool loadFullTexture = (srcX == 0 && srcY == 0 && size == bitmap->width && size == bitmap->height) || (srcX == 0 && srcY == 0 && (size > bitmap->width || size > bitmap->height));

    if (loadFullTexture)
    {
      // Full texture
      if (!texture->CreateFromBitmap(bitmap.get()))
      {
        DebugTrace("DX12Renderer::RegisterLegacySprite - Failed to create texture\n");
        return;
      }
    }
    else
    {
      // Extract sub-region - create a new RGBA buffer for just this region
      // Validate that the sub-region fits within the bitmap
      if (srcX + size > bitmap->width || srcY + size > bitmap->height)
      {
        DebugTrace("DX12Renderer::RegisterLegacySprite - Sub-region ({},{},{}) exceeds bitmap dimensions ({}x{})\n", srcX, srcY, size, bitmap->width, bitmap->height);
        return;
      }

      auto srcPixels = static_cast<const uint8_t *>(bitmap->pixels);
      std::vector<uint8_t> rgbaData(size * size * 4);
      int srcPitch = bitmap->pitch;

      for (int y = 0; y < size; y++)
      {
        const uint8_t *srcRow = srcPixels + (srcY + y) * srcPitch;
        for (int x = 0; x < size; x++)
        {
          int dstIdx = (y * size + x) * 4;

          if (bitmap->bitsPerPixel == 8 && bitmap->palette)
          {
            uint8_t paletteIndex = srcRow[srcX + x];
            if (paletteIndex < bitmap->paletteSize)
            {
              rgbaData[dstIdx + 0] = bitmap->palette[paletteIndex].rgbRed;
              rgbaData[dstIdx + 1] = bitmap->palette[paletteIndex].rgbGreen;
              rgbaData[dstIdx + 2] = bitmap->palette[paletteIndex].rgbBlue;
              rgbaData[dstIdx + 3] = 255;
            }
          }
          else if (bitmap->bitsPerPixel == 24)
          {
            int srcXOffset = (srcX + x) * 3;
            rgbaData[dstIdx + 0] = srcRow[srcXOffset + 2];
            rgbaData[dstIdx + 1] = srcRow[srcXOffset + 1];
            rgbaData[dstIdx + 2] = srcRow[srcXOffset + 0];
            rgbaData[dstIdx + 3] = 255;
          }
        }
      }

      if (!texture->Create(size, size, rgbaData.data()))
      {
        DebugTrace("DX12Renderer::RegisterLegacySprite - Failed to create sub-region texture\n");
        return;
      }
    }

    int dx12Index = static_cast<int>(sm_textures.size());
    sm_textures.push_back(std::move(texture));
    sm_legacySpriteMap[legacySpriteIndex] = dx12Index;

    DebugTrace("DX12Renderer::RegisterLegacySprite - Registered legacy sprite {} -> DX12 index {}\n", legacySpriteIndex, dx12Index);
  }

  void DX12Renderer::DrawLegacySprite(int legacySpriteIndex, float x, float y)
  {
    auto it = sm_legacySpriteMap.find(legacySpriteIndex);
    if (it == sm_legacySpriteMap.end()) return;

    int textureIndex = it->second;
    if (textureIndex < 0 || textureIndex >= static_cast<int>(sm_textures.size())) return;

    auto &tex = sm_textures[textureIndex];
    float width = static_cast<float>(tex->GetWidth());
    float height = static_cast<float>(tex->GetHeight());

    DrawSprite(textureIndex, x, y, width, height, XMFLOAT4(1, 1, 1, 1));
  }

  bool DX12Renderer::HasLegacySprite(int legacySpriteIndex) { return sm_legacySpriteMap.contains(legacySpriteIndex); }

  Texture2D *DX12Renderer::GetTexture(int index)
  {
    if (index < 0 || index >= static_cast<int>(sm_textures.size())) return nullptr;
    return sm_textures[index].get();
  }

  void DX12Renderer::ClearScreen()
  {
    auto cmdList = Core::GetCommandList();

    // Clear render target
    constexpr float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    cmdList->ClearRenderTargetView(Core::GetRenderTargetView(), clearColor, 0, nullptr);

    // Clear depth stencil 
    cmdList->ClearDepthStencilView(Core::GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
  }
}