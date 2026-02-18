#include "pch.h"
#include "WorldRenderer.h"
#include "GraphicsCommon.h"
#include "CompiledShaders/CmoVS.h"
#include "CompiledShaders/CmoPS.h"

namespace Neuron
{
  void WorldRenderer::Startup()
  {
    // CmoConstants = WorldViewProj(16) + World(16) + ObjectColor(4) = 36 root constants
    m_rootSig.Reset(1, 0);
    m_rootSig[0].InitAsConstants(0, 36);
    m_rootSig.Finalize(L"WorldRootSig",
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

    m_pso = GraphicsPSO(L"WorldPSO");
    m_pso.SetRootSignature(m_rootSig);
    m_pso.SetVertexShader(g_pCmoVS, sizeof(g_pCmoVS));
    m_pso.SetPixelShader(g_pCmoPS, sizeof(g_pCmoPS));
    m_pso.SetInputLayout(&VertexPositionNormalTexture::INPUT_LAYOUT);
    m_pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_pso.SetRenderTargetFormat(Graphics::Core::GetBackBufferFormat(),
                                Graphics::Core::GetDepthBufferFormat());
    m_pso.SetRasterizerState(Graphics::RasterizerDefault);
    m_pso.SetBlendState(Graphics::BlendDisable);
    m_pso.SetDepthStencilState(Graphics::DepthStateReadWrite);
    m_pso.Finalize();

    LoadShipMeshes();
    LoadAsteroidMeshes();

    DebugTrace("WorldRenderer started\n");
  }

  void WorldRenderer::Shutdown()
  {
    for (auto& [key, cmo] : m_cmoMeshes)
      cmo.Destroy();
    m_cmoMeshes.clear();
    DebugTrace("WorldRenderer shutdown\n");
  }

  void WorldRenderer::LoadShipMeshes()
  {
    for (uint8_t i = 0; i < static_cast<uint8_t>(ShipClass::Count); i++)
    {
      auto sc = static_cast<ShipClass>(i);
      const auto& def = GetShipDef(sc);
      uint32_t key = MeshKey(SpaceObjectType::Ship, i);

      Graphics::CmoMesh mesh;
      if (mesh.Load(def.meshFile))
      {
        m_cmoMeshes[key] = std::move(mesh);
        DebugTrace("Loaded ship mesh: {}\n", def.name);
      }
      else
      {
        DebugTrace("Failed to load ship mesh: {}\n", def.name);
      }
    }
  }

  void WorldRenderer::LoadAsteroidMeshes()
  {
    for (uint8_t i = 0; i < static_cast<uint8_t>(AsteroidClass::Count); i++)
    {
      auto ac = static_cast<AsteroidClass>(i);
      const auto& def = GetAsteroidDef(ac);
      uint32_t key = MeshKey(SpaceObjectType::Asteroid, i);

      Graphics::CmoMesh mesh;
      if (mesh.Load(def.meshFile))
      {
        m_cmoMeshes[key] = std::move(mesh);
        DebugTrace("Loaded asteroid mesh: {}\n", i);
      }
      else
      {
        DebugTrace("Failed to load asteroid mesh: {}\n", i);
      }
    }
  }

  XMFLOAT4 WorldRenderer::GetObjectColor(SpaceObjectType _type,
                                          [[maybe_unused]] uint8_t _subclass,
                                          bool _isLocal) const
  {
    if (_isLocal)
      return {1.0f, 1.0f, 0.0f, 1.0f};

    if (_type == SpaceObjectType::Asteroid)
      return {0.55f, 0.45f, 0.35f, 1.0f};

    return {1.0f, 1.0f, 1.0f, 1.0f};
  }

  uint32_t WorldRenderer::ResolveMeshKey(SpaceObjectType _type, uint8_t _subclass) const
  {
    uint32_t key = MeshKey(_type, _subclass);
    if (m_cmoMeshes.contains(key))
      return key;

    key = MeshKey(_type, 0);
    if (m_cmoMeshes.contains(key))
      return key;

    return 0xFFFF'FFFFu;
  }

  void XM_CALLCONV WorldRenderer::Render(const std::unordered_map<ObjectId, ObjectState>& _objects,
                              [[maybe_unused]] ObjectId _localPlayerId, FXMMATRIX _viewProj)
  {
    m_renderStats = {};

    if (_objects.empty())
      return;

    m_drawList.clear();
    m_drawList.reserve(_objects.size());

    for (const auto& obj : _objects | std::views::values)
    {
      uint32_t key = ResolveMeshKey(obj.type, obj.subclass);
      if (key == 0xFFFF'FFFFu)
        continue;

      if (static_cast<uint8_t>(obj.type) < static_cast<uint8_t>(SpaceObjectType::Count))
        m_renderStats.counts[static_cast<size_t>(obj.type)]++;

      XMMATRIX world = XMMatrixRotationY(obj.yaw) *
                       XMMatrixTranslation(obj.position.x, obj.position.y, obj.position.z);
      XMMATRIX wvp = world * _viewProj;

      DrawItem item;
      item.meshKey = key;
      XMStoreFloat4x4(&item.wvpTransposed, XMMatrixTranspose(wvp));
      XMStoreFloat4x4(&item.worldTransposed, XMMatrixTranspose(world));
      item.color = GetObjectColor(obj.type, obj.subclass, obj.id == _localPlayerId);
      m_drawList.push_back(item);
    }

    std::ranges::sort(m_drawList, {}, &DrawItem::meshKey);

    auto* cmdList = Graphics::Core::GetCommandList();
    cmdList->SetPipelineState(m_pso.GetPipelineStateObject());
    cmdList->SetGraphicsRootSignature(m_rootSig.GetSignature());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    uint32_t boundKey = UINT32_MAX;

    for (const auto& item : m_drawList)
    {
      if (item.meshKey != boundKey)
      {
        boundKey = item.meshKey;
        const auto& cmo = m_cmoMeshes.at(boundKey);
        cmdList->IASetVertexBuffers(0, 1, &cmo.GetVertexBufferView());
        cmdList->IASetIndexBuffer(&cmo.GetIndexBufferView());
      }

      CmoConstants constants;
      constants.WorldViewProj = item.wvpTransposed;
      constants.World = item.worldTransposed;
      constants.ObjectColor = item.color;
      cmdList->SetGraphicsRoot32BitConstants(0, 36, &constants, 0);

      const auto& cmo = m_cmoMeshes.at(boundKey);
      for (const auto& sub : cmo.GetSubmeshes())
      {
        cmdList->DrawIndexedInstanced(sub.indexCount, 1, sub.startIndex, 0, 0);
        m_renderStats.totalDrawCalls++;
      }
    }
  }
}