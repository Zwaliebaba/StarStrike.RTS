#pragma once

namespace Neuron
{
  class BitmapFont
  {
  public:
    void Load(const std::wstring& _atlasPath, uint32_t _cellWidth, uint32_t _cellHeight,
              uint32_t _gridCols = 0, uint32_t _gridRows = 0);

    uint32_t MeasureString(std::string_view _text) const;

    uint32_t GetCellWidth() const { return m_cellWidth; }
    uint32_t GetCellHeight() const { return m_cellHeight; }
    uint32_t GetLineHeight() const { return m_cellHeight + 2; }

    XMFLOAT4 GetGlyphUV(char _ch) const;
    XMFLOAT4 GetWhiteUV() const;

    D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const;

  private:
    Graphics::Texture* m_texture = nullptr;
    uint32_t m_cellWidth = 8;
    uint32_t m_cellHeight = 8;
    uint32_t m_gridCols = 16;
    uint32_t m_gridRows = 16;
  };
}
