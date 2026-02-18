#pragma once

#include "WorldTypes.h"
#include "ShipDefs.h"
#include "AsteroidDefs.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "CmoMesh.h"
#include "VertexTypes.h"
#include <unordered_map>

namespace Neuron
{
  class WorldRenderer
  {
  public:
    struct RenderStats
    {
      uint32_t counts[static_cast<size_t>(SpaceObjectType::Count)] = {};
      uint32_t totalDrawCalls = 0;
    };

    void Startup();
    void Shutdown();
    void XM_CALLCONV Render(const std::unordered_map<ObjectId, ObjectState>& _objects,
                ObjectId _localPlayerId, FXMMATRIX _viewProj);

    [[nodiscard]] const RenderStats& GetRenderStats() const noexcept { return m_renderStats; }

  private:
    void LoadShipMeshes();
    void LoadAsteroidMeshes();

    static uint32_t MeshKey(SpaceObjectType _type, uint8_t _subclass)
    {
      return (static_cast<uint32_t>(_type) << 8) | _subclass;
    }

    [[nodiscard]] uint32_t ResolveMeshKey(SpaceObjectType _type, uint8_t _subclass) const;

    XMFLOAT4 GetObjectColor(SpaceObjectType _type, uint8_t _subclass, bool _isLocal) const;

    // CMO meshes keyed by MeshKey
    std::unordered_map<uint32_t, Graphics::CmoMesh> m_cmoMeshes;

    // Per-frame draw batches sorted by mesh key to minimize VB rebinding
    struct DrawItem
    {
      uint32_t   meshKey;
      XMFLOAT4X4 wvpTransposed;
      XMFLOAT4X4 worldTransposed;
      XMFLOAT4   color;
    };
    std::vector<DrawItem> m_drawList;

    RootSignature m_rootSig;
    GraphicsPSO m_pso;

    __declspec(align(256)) struct CmoConstants
    {
      XMFLOAT4X4 WorldViewProj;
      XMFLOAT4X4 World;
      XMFLOAT4   ObjectColor;
    };

    RenderStats m_renderStats = {};
  };
}
