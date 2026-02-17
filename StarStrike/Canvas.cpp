#include "pch.h"
#include "Canvas.h"
#include "GraphicsCommon.h"
#include "CompiledShaders/CanvasVS.h"
#include "CompiledShaders/CanvasPS.h"

namespace Neuron
{
  void Canvas::Startup()
  {
    auto* device = Graphics::Core::GetD3DDevice();

    // --- Root signature ---
    m_rootSig.Reset(2, 1);
    m_rootSig[0].InitAsConstants(0, 2, D3D12_SHADER_VISIBILITY_VERTEX);
    m_rootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1,
                                        D3D12_SHADER_VISIBILITY_PIXEL);
    m_rootSig.InitStaticSampler(0, Graphics::SamplerPointClampDesc, D3D12_SHADER_VISIBILITY_PIXEL);
    m_rootSig.Finalize(L"CanvasRootSig",
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

    // --- PSO ---
    m_pso = GraphicsPSO(L"CanvasPSO");
    m_pso.SetRootSignature(m_rootSig);
    m_pso.SetVertexShader(g_pCanvasVS, sizeof(g_pCanvasVS));
    m_pso.SetPixelShader(g_pCanvasPS, sizeof(g_pCanvasPS));
    m_pso.SetInputLayout(&VertexPositionTextureColor::INPUT_LAYOUT);
    m_pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_pso.SetRenderTargetFormat(Graphics::Core::GetBackBufferFormat(), DXGI_FORMAT_UNKNOWN);
    m_pso.SetRasterizerState(Graphics::RasterizerTwoSided);
    m_pso.SetBlendState(Graphics::BlendTraditional);
    m_pso.SetDepthStencilState(Graphics::DepthStateDisabled);
    m_pso.Finalize();

    // --- Per-frame upload buffers ---
    const UINT bufferSize = MAX_VERTICES * sizeof(VertexPositionTextureColor);
    auto uploadProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    for (UINT i = 0; i < Graphics::Core::GetBackBufferCount(); ++i)
    {
      check_hresult(device->CreateCommittedResource(
        &uploadProps, D3D12_HEAP_FLAG_NONE, &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(m_uploadBuffers[i].put())));

      check_hresult(m_uploadBuffers[i]->Map(0, nullptr, &m_mappedBuffers[i]));
    }

    // --- 1x1 white texture ---
    auto srvHandle = Graphics::Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_whiteSRV = static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(srvHandle);

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    auto defaultProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    check_hresult(device->CreateCommittedResource(
      &defaultProps, D3D12_HEAP_FLAG_NONE, &texDesc,
      D3D12_RESOURCE_STATE_COMMON, nullptr,
      IID_PPV_ARGS(m_whiteTexture.put())));

    // Upload buffer for the single white pixel
    uint32_t whitePixel = 0xFFFFFFFF;
    UINT64 uploadBufferSize = 0;
    device->GetCopyableFootprints(&texDesc, 0, 1, 0, nullptr, nullptr, nullptr, &uploadBufferSize);

    auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto uploadResDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    check_hresult(device->CreateCommittedResource(
      &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadResDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(m_whiteUpload.put())));

    // Map, copy, and upload
    void* mapped = nullptr;
    check_hresult(m_whiteUpload->Map(0, nullptr, &mapped));

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, nullptr, nullptr, nullptr);
    uint8_t* destPtr = static_cast<uint8_t*>(mapped) + footprint.Offset;
    memcpy(destPtr, &whitePixel, sizeof(whitePixel));
    m_whiteUpload->Unmap(0, nullptr);

    auto* cmdList = Graphics::Core::GetCommandList();
    if (!Graphics::Core::IsCommandListOpen())
      Graphics::Core::ResetCommandAllocatorAndCommandlist();

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      m_whiteTexture.get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &barrier);

    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = m_whiteTexture.get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = m_whiteUpload.get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = footprint;

    cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
      m_whiteTexture.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdList->ResourceBarrier(1, &barrier2);

    Graphics::Core::ExecuteCommandList(true);
    Graphics::Core::WaitForGpu();
    Graphics::Core::ResetCommandAllocatorAndCommandlist();

    // Create SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(m_whiteTexture.get(), &srvDesc, srvHandle);

    m_vertices.reserve(MAX_VERTICES);

    DebugTrace("Canvas started\n");
  }

  void Canvas::Shutdown()
  {
    for (UINT i = 0; i < MAX_FRAME_COUNT; ++i)
    {
      if (m_uploadBuffers[i])
      {
        m_uploadBuffers[i]->Unmap(0, nullptr);
        m_mappedBuffers[i] = nullptr;
        m_uploadBuffers[i] = nullptr;
      }
    }
    m_whiteTexture = nullptr;
    m_whiteUpload = nullptr;
    m_vertices.clear();

    DebugTrace("Canvas shutdown\n");
  }

  void Canvas::Begin()
  {
    m_vertices.clear();
    m_currentTexture = {};
    m_bufferOffsets[Graphics::Core::GetCurrentFrameIndex()] = 0;
  }

  void Canvas::DrawRect(float _x, float _y, float _w, float _h, const XMFLOAT4& _color)
  {
    // Use white texture, UVs don't matter since texture is solid white
    XMFLOAT4 uv = {0.0f, 0.0f, 1.0f, 1.0f};
    AddQuad(_x, _y, _w, _h, uv, _color, m_whiteSRV);
  }

  void Canvas::DrawRectOutline(float _x, float _y, float _w, float _h, float _thickness,
                                const XMFLOAT4& _color)
  {
    // Top
    DrawRect(_x, _y, _w, _thickness, _color);
    // Bottom
    DrawRect(_x, _y + _h - _thickness, _w, _thickness, _color);
    // Left
    DrawRect(_x, _y + _thickness, _thickness, _h - _thickness * 2.0f, _color);
    // Right
    DrawRect(_x + _w - _thickness, _y + _thickness, _thickness, _h - _thickness * 2.0f, _color);
  }

  void Canvas::DrawText(BitmapFont& _font, float _x, float _y, std::string_view _text,
                         const XMFLOAT4& _color)
  {
    D3D12_GPU_DESCRIPTOR_HANDLE fontSRV = _font.GetSRV();
    float cursorX = _x;
    float cellW = static_cast<float>(_font.GetCellWidth());
    float cellH = static_cast<float>(_font.GetCellHeight());

    for (char ch : _text)
    {
      if (ch == ' ')
      {
        cursorX += cellW;
        continue;
      }
      XMFLOAT4 uv = _font.GetGlyphUV(ch);
      AddQuad(cursorX, _y, cellW, cellH, uv, _color, fontSRV);
      cursorX += cellW;
    }
  }

  void Canvas::DrawTextClipped(BitmapFont& _font, float _x, float _y, float _maxWidth,
                                std::string_view _text, const XMFLOAT4& _color)
  {
    D3D12_GPU_DESCRIPTOR_HANDLE fontSRV = _font.GetSRV();
    float cursorX = _x;
    float cellW = static_cast<float>(_font.GetCellWidth());
    float cellH = static_cast<float>(_font.GetCellHeight());

    for (char ch : _text)
    {
      if (cursorX + cellW > _x + _maxWidth)
        break;
      if (ch == ' ')
      {
        cursorX += cellW;
        continue;
      }
      XMFLOAT4 uv = _font.GetGlyphUV(ch);
      AddQuad(cursorX, _y, cellW, cellH, uv, _color, fontSRV);
      cursorX += cellW;
    }
  }

  void Canvas::End()
  {
    if (!m_vertices.empty())
      FlushBatch();
  }

  void Canvas::AddQuad(float _x, float _y, float _w, float _h, const XMFLOAT4& _uv,
                        const XMFLOAT4& _color, D3D12_GPU_DESCRIPTOR_HANDLE _srv)
  {
    if (m_currentTexture.ptr != _srv.ptr && !m_vertices.empty())
      FlushBatch();
    m_currentTexture = _srv;

    if (m_vertices.size() + 6 > MAX_VERTICES)
      FlushBatch();

    float x0 = _x, y0 = _y;
    float x1 = _x + _w, y1 = _y + _h;
    float u0 = _uv.x, v0 = _uv.y;
    float u1 = _uv.z, v1 = _uv.w;

    // Triangle 1: top-left, top-right, bottom-left
    m_vertices.push_back({{x0, y0, 0.0f}, {u0, v0}, _color});
    m_vertices.push_back({{x1, y0, 0.0f}, {u1, v0}, _color});
    m_vertices.push_back({{x0, y1, 0.0f}, {u0, v1}, _color});

    // Triangle 2: top-right, bottom-right, bottom-left
    m_vertices.push_back({{x1, y0, 0.0f}, {u1, v0}, _color});
    m_vertices.push_back({{x1, y1, 0.0f}, {u1, v1}, _color});
    m_vertices.push_back({{x0, y1, 0.0f}, {u0, v1}, _color});
  }

  void Canvas::FlushBatch()
  {
    if (m_vertices.empty())
      return;

    UINT frameIndex = Graphics::Core::GetCurrentFrameIndex();
    const UINT vertexCount = static_cast<UINT>(m_vertices.size());
    const UINT uploadSize = vertexCount * sizeof(VertexPositionTextureColor);
    const UINT offset = m_bufferOffsets[frameIndex];
    const UINT bufferCapacity = MAX_VERTICES * sizeof(VertexPositionTextureColor);

    if (offset + uploadSize > bufferCapacity)
    {
      m_vertices.clear();
      return;
    }

    uint8_t* dest = static_cast<uint8_t*>(m_mappedBuffers[frameIndex]) + offset;
    memcpy(dest, m_vertices.data(), uploadSize);
    m_bufferOffsets[frameIndex] = offset + uploadSize;

    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = m_uploadBuffers[frameIndex]->GetGPUVirtualAddress() + offset;
    vbView.SizeInBytes = uploadSize;
    vbView.StrideInBytes = sizeof(VertexPositionTextureColor);

    auto* cmdList = Graphics::Core::GetCommandList();
    cmdList->SetPipelineState(m_pso.GetPipelineStateObject());
    cmdList->SetGraphicsRootSignature(m_rootSig.GetSignature());

    auto viewport = Graphics::Core::GetScreenViewport();
    float invViewport[2] = {1.0f / viewport.Width, 1.0f / viewport.Height};
    cmdList->SetGraphicsRoot32BitConstants(0, 2, invViewport, 0);

    Graphics::DescriptorAllocator::SetDescriptorHeaps(cmdList);
    cmdList->SetGraphicsRootDescriptorTable(1, m_currentTexture);

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &vbView);
    cmdList->DrawInstanced(vertexCount, 1, 0, 0);

    m_vertices.clear();
  }
}
