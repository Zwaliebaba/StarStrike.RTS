#include "pch.h"
#include "WorldRenderer.h"
#include "GraphicsCommon.h"
#include "CompiledShaders/WorldVS.h"
#include "CompiledShaders/WorldPS.h"

namespace Neuron
{
  void WorldRenderer::Startup()
  {
    // Create root signature: 16 root constants at b0 (WorldViewProj = 16 floats)
    m_rootSig.Reset(1, 0);
    m_rootSig[0].InitAsConstants(0, 16, D3D12_SHADER_VISIBILITY_VERTEX);
    m_rootSig.Finalize(L"WorldRootSig",
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

    // Create PSO
    m_pso = GraphicsPSO(L"WorldPSO");
    m_pso.SetRootSignature(m_rootSig);
    m_pso.SetVertexShader(g_pWorldVS, sizeof(g_pWorldVS));
    m_pso.SetPixelShader(g_pWorldPS, sizeof(g_pWorldPS));
    m_pso.SetInputLayout(&VertexPositionColor::INPUT_LAYOUT);
    m_pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_pso.SetRenderTargetFormat(Graphics::Core::GetBackBufferFormat(),
                                Graphics::Core::GetDepthBufferFormat());
    m_pso.SetRasterizerState(Graphics::RasterizerTwoSided);
    m_pso.SetBlendState(Graphics::BlendDisable);
    m_pso.SetDepthStencilState(Graphics::DepthStateReadWrite);
    m_pso.Finalize();

    // Create meshes for known types
    CreateShipMesh(ShipClass::Asteria);
    CreateShipMesh(ShipClass::Aurora);
    CreateShipMesh(ShipClass::Avalanche);
    CreateAsteroidMesh();
    CreateDefaultMesh();

    DebugTrace("WorldRenderer started\n");
  }

  void WorldRenderer::Shutdown()
  {
    m_meshes.clear();
    DebugTrace("WorldRenderer shutdown\n");
  }

  WorldRenderer::MeshData WorldRenderer::CreateUploadedMesh(const VertexPositionColor* _vertices, uint32_t _count)
  {
    MeshData mesh;
    mesh.vertexCount = _count;

    const UINT bufferSize = _count * sizeof(VertexPositionColor);
    auto* device = Graphics::Core::GetD3DDevice();

    // Default heap
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    check_hresult(device->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(mesh.vertexBuffer.put())));

    // Upload heap
    auto uploadProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    check_hresult(device->CreateCommittedResource(
      &uploadProps, D3D12_HEAP_FLAG_NONE, &resDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(mesh.uploadBuffer.put())));

    // Map and copy
    void* mapped = nullptr;
    check_hresult(mesh.uploadBuffer->Map(0, nullptr, &mapped));
    memcpy(mapped, _vertices, bufferSize);
    mesh.uploadBuffer->Unmap(0, nullptr);

    // Copy
    auto* cmdList = Graphics::Core::GetCommandList();
    if (!Graphics::Core::IsCommandListOpen())
      Graphics::Core::ResetCommandAllocatorAndCommandlist();

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      mesh.vertexBuffer.get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &barrier);
    cmdList->CopyResource(mesh.vertexBuffer.get(), mesh.uploadBuffer.get());
    auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
      mesh.vertexBuffer.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    cmdList->ResourceBarrier(1, &barrier2);

    Graphics::Core::ExecuteCommandList(true);
    Graphics::Core::WaitForGpu();
    Graphics::Core::ResetCommandAllocatorAndCommandlist();

    mesh.vbView.BufferLocation = mesh.vertexBuffer->GetGPUVirtualAddress();
    mesh.vbView.SizeInBytes = bufferSize;
    mesh.vbView.StrideInBytes = sizeof(VertexPositionColor);

    return mesh;
  }

  void WorldRenderer::CreateShipMesh(ShipClass _class)
  {
    // 80s-style flat-colored ship shapes
    XMFLOAT4 color;
    float scale;

    switch (_class)
    {
    case ShipClass::Asteria:
      color = {0.2f, 0.8f, 0.2f, 1.0f}; // green
      scale = 5.0f;
      break;
    case ShipClass::Aurora:
      color = {0.2f, 0.4f, 1.0f, 1.0f}; // blue
      scale = 4.0f;
      break;
    case ShipClass::Avalanche:
      color = {1.0f, 0.3f, 0.1f, 1.0f}; // red-orange
      scale = 7.0f;
      break;
    default:
      color = {1.0f, 1.0f, 1.0f, 1.0f};
      scale = 5.0f;
      break;
    }

    XMFLOAT4 darkColor = {color.x * 0.5f, color.y * 0.5f, color.z * 0.5f, 1.0f};
    XMFLOAT4 lightColor = {(std::min)(color.x * 1.3f, 1.0f), (std::min)(color.y * 1.3f, 1.0f),
                           (std::min)(color.z * 1.3f, 1.0f), 1.0f};

    // Arrow/wedge shape pointing +Z
    float s = scale;
    VertexPositionColor vertices[] =
    {
      // Top face (2 triangles forming a diamond)
      {{0.0f,    0.5f * s,  s * 2.0f}, lightColor},  // nose
      {{-s,      0.0f,      -s},       color},        // left back
      {{s,       0.0f,      -s},       color},        // right back

      // Bottom face
      {{0.0f,   -0.3f * s,  s * 2.0f}, darkColor},
      {{s,       0.0f,      -s},       darkColor},
      {{-s,      0.0f,      -s},       darkColor},

      // Left side
      {{0.0f,    0.5f * s,  s * 2.0f}, color},
      {{0.0f,   -0.3f * s,  s * 2.0f}, darkColor},
      {{-s,      0.0f,      -s},       darkColor},

      // Right side
      {{0.0f,    0.5f * s,  s * 2.0f}, color},
      {{s,       0.0f,      -s},       darkColor},
      {{0.0f,   -0.3f * s,  s * 2.0f}, darkColor},

      // Back face
      {{-s,      0.0f,      -s},       darkColor},
      {{0.0f,   -0.3f * s,  s * -0.5f}, darkColor},
      {{s,       0.0f,      -s},       darkColor},

      {{-s,      0.0f,      -s},       color},
      {{s,       0.0f,      -s},       color},
      {{0.0f,    0.5f * s,  s * -0.5f}, color},
    };

    uint32_t key = MeshKey(SpaceObjectType::Ship, static_cast<uint8_t>(_class));
    m_meshes[key] = CreateUploadedMesh(vertices, _countof(vertices));
  }

  void WorldRenderer::CreateAsteroidMesh()
  {
    XMFLOAT4 color = {0.5f, 0.45f, 0.4f, 1.0f};
    XMFLOAT4 dark = {0.35f, 0.3f, 0.25f, 1.0f};
    float s = 6.0f;

    // Simple octahedron
    XMFLOAT3 top    = {0, s, 0};
    XMFLOAT3 bottom = {0, -s, 0};
    XMFLOAT3 front  = {0, 0, s};
    XMFLOAT3 back   = {0, 0, -s};
    XMFLOAT3 left   = {-s, 0, 0};
    XMFLOAT3 right  = {s, 0, 0};

    VertexPositionColor vertices[] =
    {
      // Top 4 faces
      {top, color}, {front, color}, {right, dark},
      {top, color}, {right, dark},  {back, color},
      {top, dark},  {back, dark},   {left, color},
      {top, dark},  {left, color},  {front, dark},
      // Bottom 4 faces
      {bottom, dark}, {right, color}, {front, dark},
      {bottom, dark}, {back, color},  {right, dark},
      {bottom, color},{left, dark},   {back, color},
      {bottom, color},{front, color}, {left, dark},
    };

    uint32_t key = MeshKey(SpaceObjectType::Asteroid, 0);
    m_meshes[key] = CreateUploadedMesh(vertices, _countof(vertices));
  }

  void WorldRenderer::CreateDefaultMesh()
  {
    XMFLOAT4 color = {0.8f, 0.8f, 0.8f, 1.0f};
    float s = 3.0f;

    // Simple box
    VertexPositionColor vertices[] =
    {
      // Front
      {{-s, -s,  s}, color}, {{ s, -s,  s}, color}, {{ s,  s,  s}, color},
      {{-s, -s,  s}, color}, {{ s,  s,  s}, color}, {{-s,  s,  s}, color},
      // Back
      {{ s, -s, -s}, color}, {{-s, -s, -s}, color}, {{-s,  s, -s}, color},
      {{ s, -s, -s}, color}, {{-s,  s, -s}, color}, {{ s,  s, -s}, color},
      // Top
      {{-s,  s,  s}, color}, {{ s,  s,  s}, color}, {{ s,  s, -s}, color},
      {{-s,  s,  s}, color}, {{ s,  s, -s}, color}, {{-s,  s, -s}, color},
      // Bottom
      {{-s, -s, -s}, color}, {{ s, -s, -s}, color}, {{ s, -s,  s}, color},
      {{-s, -s, -s}, color}, {{ s, -s,  s}, color}, {{-s, -s,  s}, color},
    };

    // Use a "fallback" key
    uint32_t key = 0xFFFF;
    m_meshes[key] = CreateUploadedMesh(vertices, _countof(vertices));
  }

  XMFLOAT4 WorldRenderer::GetObjectColor([[maybe_unused]] SpaceObjectType _type,
                                          [[maybe_unused]] uint8_t _subclass,
                                          bool _isLocal) const
  {
    if (_isLocal)
      return {1.0f, 1.0f, 0.0f, 1.0f}; // highlight local player
    return {1.0f, 1.0f, 1.0f, 1.0f};
  }

  void XM_CALLCONV WorldRenderer::Render(const std::unordered_map<ObjectId, ObjectState>& _objects,
                              [[maybe_unused]] ObjectId _localPlayerId, FXMMATRIX _viewProj)
  {
    auto* cmdList = Graphics::Core::GetCommandList();

    cmdList->SetPipelineState(m_pso.GetPipelineStateObject());
    cmdList->SetGraphicsRootSignature(m_rootSig.GetSignature());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (const auto &obj: _objects | std::views::values)
    {
      // Find mesh
      uint32_t key = MeshKey(obj.type, obj.subclass);
      auto it = m_meshes.find(key);
      if (it == m_meshes.end())
      {
        // Try type-only
        key = MeshKey(obj.type, 0);
        it = m_meshes.find(key);
        if (it == m_meshes.end())
        {
          // Fallback
          key = 0xFFFF;
          it = m_meshes.find(key);
          if (it == m_meshes.end())
            continue;
        }
      }

      const auto& mesh = it->second;

      // Build world matrix
      XMMATRIX world = XMMatrixRotationY(obj.yaw) *
                       XMMatrixTranslation(obj.position.x, obj.position.y, obj.position.z);
      XMMATRIX wvp = world * _viewProj;

      // Transpose for HLSL column-major
      XMFLOAT4X4 wvpT;
      XMStoreFloat4x4(&wvpT, XMMatrixTranspose(wvp));

      cmdList->SetGraphicsRoot32BitConstants(0, 16, &wvpT, 0);
      cmdList->IASetVertexBuffers(0, 1, &mesh.vbView);
      cmdList->DrawInstanced(mesh.vertexCount, 1, 0, 0);
    }
  }
}
