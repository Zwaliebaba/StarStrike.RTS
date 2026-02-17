#pragma once

#include "RootSignature.h"
#include "PipelineState.h"
#include "VertexTypes.h"
#include "Texture.h"

namespace Neuron
{
  class SkyBox
  {
  public:
    void Startup(const std::wstring& _texturePath);
    void Shutdown();
    void XM_CALLCONV Render(FXMMATRIX _view, CXMMATRIX _projection);

  private:
    void CreateCubeMesh();

    RootSignature m_rootSig;
    GraphicsPSO   m_pso;
    Graphics::Texture* m_texture = nullptr;

    com_ptr<ID3D12Resource> m_vertexBuffer;
    com_ptr<ID3D12Resource> m_uploadBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vbView = {};
    uint32_t m_vertexCount = 0;

    __declspec(align(256)) struct SkyBoxConstants
    {
      XMFLOAT4X4 ViewProj;
    };
  };
}
