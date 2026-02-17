#pragma once

#include "BitmapFont.h"

namespace Neuron
{
  class Canvas
  {
  public:
    void Startup();
    void Shutdown();

    void Begin();

    void DrawRect(float _x, float _y, float _w, float _h, const XMFLOAT4& _color);
    void DrawRectOutline(float _x, float _y, float _w, float _h, float _thickness,
                         const XMFLOAT4& _color);
    void DrawText(BitmapFont& _font, float _x, float _y, std::string_view _text,
                  const XMFLOAT4& _color);
    void DrawTextClipped(BitmapFont& _font, float _x, float _y, float _maxWidth,
                         std::string_view _text, const XMFLOAT4& _color);

    void End();

  private:
    void FlushBatch();
    void AddQuad(float _x, float _y, float _w, float _h, const XMFLOAT4& _uv,
                 const XMFLOAT4& _color, D3D12_GPU_DESCRIPTOR_HANDLE _srv);

    RootSignature m_rootSig;
    GraphicsPSO   m_pso;

    static constexpr uint32_t MAX_QUADS = 4096;
    static constexpr uint32_t MAX_VERTICES = MAX_QUADS * 6;
    std::vector<VertexPositionTextureColor> m_vertices;
    D3D12_GPU_DESCRIPTOR_HANDLE m_currentTexture = {};

    static constexpr uint32_t MAX_FRAME_COUNT = 3;
    com_ptr<ID3D12Resource> m_uploadBuffers[MAX_FRAME_COUNT];
    void* m_mappedBuffers[MAX_FRAME_COUNT] = {};
    uint32_t m_bufferOffsets[MAX_FRAME_COUNT] = {};

    D3D12_GPU_DESCRIPTOR_HANDLE m_whiteSRV = {};
    com_ptr<ID3D12Resource> m_whiteTexture;
    com_ptr<ID3D12Resource> m_whiteUpload;
  };
}
