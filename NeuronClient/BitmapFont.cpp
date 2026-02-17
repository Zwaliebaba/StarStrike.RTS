#include "pch.h"
#include "BitmapFont.h"

namespace Neuron
{
  void BitmapFont::Load(const std::wstring& _atlasPath, uint32_t _cellWidth, uint32_t _cellHeight,
                        uint32_t _gridCols, uint32_t _gridRows)
  {
    m_cellWidth = _cellWidth;
    m_cellHeight = _cellHeight;

    m_texture = Graphics::TextureManager::LoadFromFile(_atlasPath);
    ASSERT_TEXT(m_texture != nullptr, L"BitmapFont: Failed to load atlas\n");

    // Derive grid dimensions from actual texture size if defaults were passed
    if (_gridCols == 0 || _gridRows == 0)
    {
      m_gridCols = m_texture->GetWidth() / m_cellWidth;
      m_gridRows = m_texture->GetHeight() / m_cellHeight;
    }
    else
    {
      // Verify caller's grid matches the texture — override with actual if mismatched
      uint32_t actualCols = m_texture->GetWidth() / m_cellWidth;
      uint32_t actualRows = m_texture->GetHeight() / m_cellHeight;
      if (actualCols != _gridCols || actualRows != _gridRows)
      {
        DebugTrace("BitmapFont: grid mismatch! caller={}x{} actual={}x{} (tex={}x{}, cell={}x{}) — using actual\n",
                   _gridCols, _gridRows, actualCols, actualRows,
                   m_texture->GetWidth(), m_texture->GetHeight(), m_cellWidth, m_cellHeight);
        m_gridCols = actualCols;
        m_gridRows = actualRows;
      }
      else
      {
        m_gridCols = _gridCols;
        m_gridRows = _gridRows;
      }
    }

    ASSERT_TEXT(m_gridCols > 0 && m_gridRows > 0, L"BitmapFont: invalid grid dimensions\n");

    DebugTrace("BitmapFont loaded: {} ({}x{} tex, {}x{} grid, {}x{} cell)\n",
               winrt::to_string(_atlasPath),
               m_texture->GetWidth(), m_texture->GetHeight(),
               m_gridCols, m_gridRows, m_cellWidth, m_cellHeight);
  }

  uint32_t BitmapFont::MeasureString(std::string_view _text) const
  {
    return static_cast<uint32_t>(_text.size()) * m_cellWidth;
  }

  XMFLOAT4 BitmapFont::GetGlyphUV(char _ch) const
  {
    int index = static_cast<int>(static_cast<unsigned char>(_ch)) - 32;
    if (index < 0) index = 0;
    int maxIndex = static_cast<int>(m_gridCols * m_gridRows) - 1;
    if (index > maxIndex) index = maxIndex;

    uint32_t col = static_cast<uint32_t>(index) % m_gridCols;
    uint32_t row = static_cast<uint32_t>(index) / m_gridCols;

    float u0 = static_cast<float>(col) / static_cast<float>(m_gridCols);
    float v0 = static_cast<float>(row) / static_cast<float>(m_gridRows);
    float u1 = static_cast<float>(col + 1) / static_cast<float>(m_gridCols);
    float v1 = static_cast<float>(row + 1) / static_cast<float>(m_gridRows);

    return {u0, v0, u1, v1};
  }

  XMFLOAT4 BitmapFont::GetWhiteUV() const
  {
    // Use the space character glyph (index 0) as a solid region
    return GetGlyphUV(' ');
  }

  D3D12_GPU_DESCRIPTOR_HANDLE BitmapFont::GetSRV() const
  {
    ASSERT(m_texture != nullptr);
    return m_texture->GetSRV();
  }
}
