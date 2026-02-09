#pragma once

#include "Texture.h"
#include "VertexTypes.h"

namespace Neuron::Graphics
{
  //=============================================================================
  // GlyphMetrics - Per-character metrics for font rendering
  //=============================================================================
  struct GlyphMetrics
  {
    float u0, v0;           // Top-left UV
    float u1, v1;           // Bottom-right UV
    float width;            // Glyph width in pixels
    float height;           // Glyph height in pixels
    float advance;          // Horizontal advance to next character
    float bearingX;         // Horizontal offset from cursor
    float bearingY;         // Vertical offset from baseline
  };

  //=============================================================================
  // FontPrint - Glyph atlas and text measurement for Canvas
  //
  // Responsibilities:
  // - Load font texture atlas and metrics
  // - Measure string widths
  // - Emit glyph quads into vertex batches
  //
  // Does NOT:
  // - Issue DX12 draw calls directly
  // - Own GPU resources (caller manages the texture)
  //=============================================================================
  class FontPrint
  {
  public:
    FontPrint() = default;
    ~FontPrint() = default;

    // Non-copyable, movable
    FontPrint(const FontPrint&) = delete;
    FontPrint& operator=(const FontPrint&) = delete;
    FontPrint(FontPrint&&) = default;
    FontPrint& operator=(FontPrint&&) = default;

    //-------------------------------------------------------------------------
    // Initialization
    //-------------------------------------------------------------------------

    /// Load font from texture atlas with pre-computed metrics
    /// @param _textureIndex Texture index for the font atlas
    /// @param _atlasWidth Width of the atlas texture in pixels
    /// @param _atlasHeight Height of the atlas texture in pixels
    /// @param _glyphMetrics Per-character metrics (256 entries for extended ASCII)
    /// @param _lineHeight Height of a line of text in pixels
    bool Initialize(int _textureIndex, uint32_t _atlasWidth, uint32_t _atlasHeight,
                    std::vector<GlyphMetrics>&& _glyphMetrics, float _lineHeight);

    /// Load font from a grid-based DDS texture atlas
    /// Standard layout: characters arranged in rows, starting at specified ASCII code
    /// @param _textureIndex Texture index for the font atlas
    /// @param _atlasWidth Width of the atlas texture in pixels
    /// @param _atlasHeight Height of the atlas texture in pixels
    /// @param _glyphWidth Width of each glyph cell in pixels
    /// @param _glyphHeight Height of each glyph cell in pixels
    /// @param _charsPerRow Number of characters per row in the atlas
    /// @param _firstChar First ASCII character in the atlas (default 32 = space)
    bool LoadFromGrid(int _textureIndex, uint32_t _atlasWidth, uint32_t _atlasHeight,
                      uint32_t _glyphWidth, uint32_t _glyphHeight, uint32_t _charsPerRow,
                      uint32_t _firstChar = 32);

    //-------------------------------------------------------------------------
    // Metrics
    //-------------------------------------------------------------------------

    /// Measure the width of a text string in pixels
    [[nodiscard]] float MeasureWidth(const char* _text, float _scale = 1.0f) const;

    /// Get the line height in pixels
    [[nodiscard]] float GetLineHeight(float _scale = 1.0f) const { return m_lineHeight * _scale; }

    /// Get the glyph metrics for a character
    [[nodiscard]] const GlyphMetrics* GetGlyph(char _ch) const;

    //-------------------------------------------------------------------------
    // Quad emission
    //-------------------------------------------------------------------------

    /// Emit glyph quads for a text string
    /// @param _vertices Output vector to append vertices to
    /// @param _x Starting X position in screen coordinates
    /// @param _y Starting Y position in screen coordinates
    /// @param _z Z depth for layering
    /// @param _text Text string to render
    /// @param _color Text color (RGBA)
    /// @param _scale Scale factor
    /// @return Final X position after text
    float EmitGlyphs(std::vector<VertexPositionTextureColor>& _vertices,
                     float _x, float _y, float _z,
                     const char* _text, FXMVECTOR _color, float _scale = 1.0f) const;

    //-------------------------------------------------------------------------
    // Accessors
    //-------------------------------------------------------------------------

    [[nodiscard]] int GetTextureIndex() const { return m_textureIndex; }
    [[nodiscard]] bool IsValid() const { return m_textureIndex >= 0 && !m_glyphs.empty(); }

  private:
    int m_textureIndex = -1;
    uint32_t m_atlasWidth = 0;
    uint32_t m_atlasHeight = 0;
    float m_lineHeight = 0.0f;
    std::vector<GlyphMetrics> m_glyphs;  // 256 entries for extended ASCII

    // Helper to compute glyph metrics from a fixed-width grid layout
    void ComputeGridMetrics(uint32_t _glyphWidth, uint32_t _glyphHeight, 
                            uint32_t _charsPerRow, uint32_t _firstChar);
  };

} // namespace Neuron::Graphics
