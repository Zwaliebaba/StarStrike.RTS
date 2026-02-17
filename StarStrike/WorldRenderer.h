#pragma once

#include "WorldTypes.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "VertexTypes.h"
#include <unordered_map>

namespace Neuron
{
  class WorldRenderer
  {
  public:
    void Startup();
    void Shutdown();
    void XM_CALLCONV Render(const std::unordered_map<ObjectId, ObjectState>& _objects,
                ObjectId _localPlayerId, FXMMATRIX _viewProj);

  private:
    struct MeshData
    {
      com_ptr<ID3D12Resource> vertexBuffer;
      com_ptr<ID3D12Resource> uploadBuffer;
      D3D12_VERTEX_BUFFER_VIEW vbView = {};
      uint32_t vertexCount = 0;
    };

    void CreateShipMesh(ShipClass _class);
    void CreateAsteroidMesh();
    void CreateDefaultMesh();
    MeshData CreateUploadedMesh(const VertexPositionColor* _vertices, uint32_t _count);

    uint32_t MeshKey(SpaceObjectType _type, uint8_t _subclass) const
    {
      return (static_cast<uint32_t>(_type) << 8) | _subclass;
    }

    XMFLOAT4 GetObjectColor(SpaceObjectType _type, uint8_t _subclass, bool _isLocal) const;

    std::unordered_map<uint32_t, MeshData> m_meshes;
    RootSignature m_rootSig;
    GraphicsPSO m_pso;

    __declspec(align(256)) struct WorldConstants
    {
      XMFLOAT4X4 WorldViewProj;
    };
  };
}
