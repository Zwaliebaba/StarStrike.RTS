#include "pch.h"
#include "FontPrint.h"

namespace Neuron::Graphics
{
  //=============================================================================
  // FontPrint Implementation
  //=============================================================================

  bool FontPrint::Initialize(int _textureIndex, uint32_t _atlasWidth, uint32_t _atlasHeight, std::vector<GlyphMetrics> &&_glyphMetrics, float _lineHeight)
  {
    if (_glyphMetrics.size() < 256)
    {
      DebugTrace("FontPrint::Initialize - Invalid glyph metrics count: {}\n", _glyphMetrics.size());
      return false;
    }

    m_textureIndex = _textureIndex;
    m_atlasWidth = _atlasWidth;
    m_atlasHeight = _atlasHeight;
    m_glyphs = std::move(_glyphMetrics);
    m_lineHeight = _lineHeight;

    DebugTrace("FontPrint initialized: atlas {}x{}, lineHeight={}\n", m_atlasWidth, m_atlasHeight, m_lineHeight);
    return true;
  }

  bool FontPrint::LoadFromGrid(int _textureIndex, uint32_t _atlasWidth, uint32_t _atlasHeight, uint32_t _glyphWidth, uint32_t _glyphHeight, uint32_t _charsPerRow, uint32_t _firstChar)
  {
    if (_atlasWidth == 0 || _atlasHeight == 0 || _glyphWidth == 0 || _glyphHeight == 0 || _charsPerRow == 0)
    {
      DebugTrace("FontPrint::LoadFromGrid - Invalid dimensions\n");
      return false;
    }

    m_textureIndex = _textureIndex;
    m_atlasWidth = _atlasWidth;
    m_atlasHeight = _atlasHeight;
    m_lineHeight = static_cast<float>(_glyphHeight);

    // Compute glyph metrics from the grid layout
    ComputeGridMetrics(_glyphWidth, _glyphHeight, _charsPerRow, _firstChar);

    DebugTrace("FontPrint loaded from grid: atlas {}x{}, glyph {}x{}, {} chars/row, firstChar={}\n", m_atlasWidth, m_atlasHeight, _glyphWidth, _glyphHeight, _charsPerRow, _firstChar);
    return true;
  }

  void FontPrint::ComputeGridMetrics(uint32_t _glyphWidth, uint32_t _glyphHeight, uint32_t _charsPerRow, uint32_t _firstChar)
  {
    m_glyphs.resize(256);

    float invAtlasW = 1.0f / static_cast<float>(m_atlasWidth);
    float invAtlasH = 1.0f / static_cast<float>(m_atlasHeight);
    float glyphW = static_cast<float>(_glyphWidth);
    float glyphH = static_cast<float>(_glyphHeight);

    // Calculate total characters that fit in the atlas
    uint32_t rows = m_atlasHeight / _glyphHeight;
    uint32_t totalChars = rows * _charsPerRow;

    for (uint32_t ch = 0; ch < 256; ++ch)
    {
      auto &glyph = m_glyphs[ch];

      // Calculate grid index for this character
      if (ch < _firstChar)
      {
        // Character before the first one in atlas - use space metrics (no visible glyph)
        glyph = {};
        glyph.width = glyphW;
        glyph.height = glyphH;
        glyph.advance = glyphW;
        glyph.bearingX = 0.0f;
        glyph.bearingY = 0.0f;
        continue;
      }

      uint32_t gridIndex = ch - _firstChar;
      if (gridIndex >= totalChars)
      {
        // Character beyond what's in the atlas - use default (no visible glyph)
        glyph = {};
        glyph.width = glyphW;
        glyph.height = glyphH;
        glyph.advance = glyphW;
        glyph.bearingX = 0.0f;
        glyph.bearingY = 0.0f;
        continue;
      }

      uint32_t col = gridIndex % _charsPerRow;
      uint32_t row = gridIndex / _charsPerRow;

      float pixelX = static_cast<float>(col) * glyphW;
      float pixelY = static_cast<float>(row) * glyphH;

      glyph.u0 = pixelX * invAtlasW;
      glyph.v0 = pixelY * invAtlasH;
      glyph.u1 = (pixelX + glyphW) * invAtlasW;
      glyph.v1 = (pixelY + glyphH) * invAtlasH;

      glyph.width = glyphW;
      glyph.height = glyphH;
      glyph.advance = glyphW;  // Monospace font - all chars same width
      glyph.bearingX = 0.0f;
      glyph.bearingY = 0.0f;
    }
  }

  float FontPrint::MeasureWidth(const char *_text, float _scale) const
  {
    if (!_text || !IsValid()) return 0.0f;

    float width = 0.0f;
    for (const char *p = _text; *p != '\0'; ++p)
    {
      unsigned char ch = static_cast<unsigned char>(*p);
      const auto &glyph = m_glyphs[ch];
      width += glyph.advance * _scale;
    }
    return width;
  }

  const GlyphMetrics *FontPrint::GetGlyph(char _ch) const
  {
    unsigned char ch = static_cast<unsigned char>(_ch);
    if (ch >= m_glyphs.size()) return nullptr;
    return &m_glyphs[ch];
  }

  float FontPrint::EmitGlyphs(std::vector<VertexPositionTextureColor> &_vertices, float _x, float _y, float _z, const char *_text, FXMVECTOR _color, float _scale) const
  {
    if (!_text || !IsValid()) return _x;

    XMFLOAT4 colorF;
    XMStoreFloat4(&colorF, _color);

    float cursorX = _x;

    for (const char *p = _text; *p != '\0'; ++p)
    {
      unsigned char ch = static_cast<unsigned char>(*p);
      const auto &glyph = m_glyphs[ch];

      // Check if glyph has valid UV coordinates (non-zero UV rect indicates a renderable glyph)
      bool hasValidUV = (glyph.u1 > glyph.u0) && (glyph.v1 > glyph.v0);

      // Skip rendering for space-like characters (no UV) but still advance cursor
      if (glyph.width <= 0.0f || !hasValidUV)
      {
        cursorX += glyph.advance * _scale;
        continue;
      }

      float x0 = cursorX + glyph.bearingX * _scale;
      float y0 = _y + glyph.bearingY * _scale;
      float x1 = x0 + glyph.width * _scale;
      float y1 = y0 + glyph.height * _scale;

      // Emit two triangles (6 vertices) for this glyph
      // Triangle 1: top-left, top-right, bottom-left
      _vertices.emplace_back(XMFLOAT3{x0, y0, _z}, XMFLOAT2{glyph.u0, glyph.v0}, colorF);
      _vertices.emplace_back(XMFLOAT3{x1, y0, _z}, XMFLOAT2{glyph.u1, glyph.v0}, colorF);
      _vertices.emplace_back(XMFLOAT3{x0, y1, _z}, XMFLOAT2{glyph.u0, glyph.v1}, colorF);

      // Triangle 2: top-right, bottom-right, bottom-left
      _vertices.emplace_back(XMFLOAT3{x1, y0, _z}, XMFLOAT2{glyph.u1, glyph.v0}, colorF);
      _vertices.emplace_back(XMFLOAT3{x1, y1, _z}, XMFLOAT2{glyph.u1, glyph.v1}, colorF);
      _vertices.emplace_back(XMFLOAT3{x0, y1, _z}, XMFLOAT2{glyph.u0, glyph.v1}, colorF);

      cursorX += glyph.advance * _scale;
    }

    return cursorX;
  }

}// namespace Neuron::Graphics