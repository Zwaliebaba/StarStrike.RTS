#pragma once

#include "PipelineState.h"
#include "RootSignature.h"
#include "VertexTypes.h"
#include "Texture.h"
#include "FontPrint.h"
#include "DescriptorHeap.h"

namespace Neuron::Graphics
{
  // Opaque font identifier
  using FontId = int;

  // Invalid font constant
  constexpr FontId FONT_INVALID = -1;

  // Fixed render target size for Canvas
  constexpr uint32_t CANVAS_WIDTH = 1920;
  constexpr uint32_t CANVAS_HEIGHT = 1080;

  // Default font grid parameters (8x8 monospace starting at ASCII 32)
  constexpr uint32_t DEFAULT_GLYPH_WIDTH = 8;
  constexpr uint32_t DEFAULT_GLYPH_HEIGHT = 8;
  constexpr uint32_t DEFAULT_CHARS_PER_ROW = 16;
  constexpr uint32_t DEFAULT_FIRST_CHAR = 32;

  // Constant buffer for Canvas rendering (256-byte aligned)
  __declspec(align(256)) struct CanvasConstants
  {
    XMFLOAT4X4 Projection;
  };

  //=============================================================================
  // Canvas - High-performance 2D rendering backend
  //
  // Design principles:
  // - Rendering backend only (no input, layout, or widget state)
  // - Retained draw-command recorder with batching
  // - Explicit layering (Z) and clipping (scissor stack)
  // - Renders to a fixed-size texture (1920x1080) for compositing
  //=============================================================================
  class Canvas
  {
  public:
    //-------------------------------------------------------------------------
    // Lifecycle
    //-------------------------------------------------------------------------
    static void Startup();
    static void Shutdown();

    //-------------------------------------------------------------------------
    // Frame control
    //-------------------------------------------------------------------------
    static void BeginFrame();
    static void Render();  // Emits all recorded DX12 commands

    //-------------------------------------------------------------------------
    // Coordinate system
    //-------------------------------------------------------------------------
    [[nodiscard]] static float GetVirtualWidth() { return static_cast<float>(CANVAS_WIDTH); }
    [[nodiscard]] static float GetVirtualHeight() { return static_cast<float>(CANVAS_HEIGHT); }

    //-------------------------------------------------------------------------
    // Clipping (scissor stack)
    //-------------------------------------------------------------------------
    static void PushClipRect(const RECT& _rectPixels);
    static void PopClipRect();
    static void ResetClipStack();

    //-------------------------------------------------------------------------
    // Layering (Z-order stack)
    //-------------------------------------------------------------------------
    static void PushLayer(float _z);
    static void PopLayer();
    [[nodiscard]] static float GetCurrentLayer() { return sm_currentZ; }

    //-------------------------------------------------------------------------
    // Primitive rendering (batched, untextured)
    //-------------------------------------------------------------------------
    static void XM_CALLCONV DrawLine(float _x1, float _y1, float _x2, float _y2, FXMVECTOR _color);
    static void XM_CALLCONV DrawRectangle(float _left, float _top, float _right, float _bottom, FXMVECTOR _color);
    static void XM_CALLCONV DrawRectangleOutline(float _left, float _top, float _right, float _bottom, float _thickness, FXMVECTOR _color);
    static void XM_CALLCONV DrawTriangle(float _x1, float _y1, float _x2, float _y2, float _x3, float _y3, FXMVECTOR _color);
    static void XM_CALLCONV DrawCircle(float _cx, float _cy, float _radius, FXMVECTOR _color, int _segments = 32);
    static void XM_CALLCONV DrawCircleOutline(float _cx, float _cy, float _radius, float _thickness, FXMVECTOR _color, int _segments = 32);

    //-------------------------------------------------------------------------
    // Sprite/texture rendering (batched)
    //-------------------------------------------------------------------------
    static int LoadTexture(const std::wstring& _filename);
    static void XM_CALLCONV DrawSprite(int _textureIndex, float _x, float _y, float _width, float _height, FXMVECTOR _tint = g_XMOne);
    static void XM_CALLCONV DrawSpriteUV(int _textureIndex, float _x, float _y, float _width, float _height,
                                         float _u0, float _v0, float _u1, float _v1, FXMVECTOR _tint = g_XMOne);
    static void XM_CALLCONV DrawSpriteRotated(int _textureIndex, float _cx, float _cy, float _width, float _height,
                                              float _rotation, FXMVECTOR _tint = g_XMOne);

    //-------------------------------------------------------------------------
    // Text rendering (integrated FontPrint)
    //-------------------------------------------------------------------------
    
    /// Load a grid-based font from a DDS texture
    /// @param _fontTexture Path to the DDS font atlas texture
    /// @param _glyphWidth Width of each glyph cell (default 8)
    /// @param _glyphHeight Height of each glyph cell (default 8)
    /// @param _charsPerRow Characters per row in atlas (default 16)
    /// @param _firstChar First ASCII character in atlas (default 32 = space)
    static FontId LoadFont(const std::wstring& _fontTexture, 
                           uint32_t _glyphWidth = DEFAULT_GLYPH_WIDTH,
                           uint32_t _glyphHeight = DEFAULT_GLYPH_HEIGHT,
                           uint32_t _charsPerRow = DEFAULT_CHARS_PER_ROW,
                           uint32_t _firstChar = DEFAULT_FIRST_CHAR);
    
    static void XM_CALLCONV DrawText(FontId _fontId, float _x, float _y, const char* _text, FXMVECTOR _color, float _scale = 1.0f);
    static void XM_CALLCONV DrawTextCentered(FontId _fontId, float _centerX, float _y, const char* _text, FXMVECTOR _color, float _scale = 1.0f);
    static void XM_CALLCONV DrawTextRight(FontId _fontId, float _rightX, float _y, const char* _text, FXMVECTOR _color, float _scale = 1.0f);
    [[nodiscard]] static float MeasureTextWidth(FontId _fontId, const char* _text, float _scale = 1.0f);
    [[nodiscard]] static float GetFontHeight(FontId _fontId, float _scale = 1.0f);

    //-------------------------------------------------------------------------
    // Widget helpers (rendering only - no state)
    //-------------------------------------------------------------------------
    static void XM_CALLCONV DrawPanel(const RECT& _rect, FXMVECTOR _color);
    static void XM_CALLCONV DrawPanelBordered(const RECT& _rect, FXMVECTOR _fillColor, FXMVECTOR _borderColor, float _borderThickness = 1.0f);
    static void XM_CALLCONV DrawImage(const RECT& _rect, int _textureIndex, FXMVECTOR _tint = g_XMOne);
    static void XM_CALLCONV DrawBorder(const RECT& _rect, float _thickness, FXMVECTOR _color);

    //-------------------------------------------------------------------------
    // Resource accessors
    //-------------------------------------------------------------------------
    [[nodiscard]] static Texture* GetTexture(int _index);
    [[nodiscard]] static int GetTextureCount() { return static_cast<int>(sm_textures.size()); }

    //-------------------------------------------------------------------------
    // Render target access (for compositing onto 3D scene)
    //-------------------------------------------------------------------------
    [[nodiscard]] static D3D12_GPU_DESCRIPTOR_HANDLE GetRenderTargetSRV() { return sm_renderTargetSRV; }
    [[nodiscard]] static bool IsValid() { return sm_renderTarget.get() != nullptr; }
    [[nodiscard]] static uint32_t GetWidth() { return CANVAS_WIDTH; }
    [[nodiscard]] static uint32_t GetHeight() { return CANVAS_HEIGHT; }

  private:
    //-------------------------------------------------------------------------
    // Internal helpers
    //-------------------------------------------------------------------------
    static void CreateRenderTarget();
    static void CreateRootSignature();
    static void CreatePipelineStates();
    static void CreateDynamicBuffers();
    static void UpdateProjection();

    static void FlushPrimitives();
    static void FlushSprites();
    static void ApplyClipRect(const RECT& _rect);

    static void RecordSpriteQuad(float _x, float _y, float _width, float _height,
                                 float _u0, float _v0, float _u1, float _v1, FXMVECTOR _color);

    //-------------------------------------------------------------------------
    // Root signature and PSOs
    //-------------------------------------------------------------------------
    inline static RootSignature sm_primitiveRootSig;
    inline static RootSignature sm_spriteRootSig;
    inline static GraphicsPSO sm_linePSO{L"Canvas Line PSO"};
    inline static GraphicsPSO sm_trianglePSO{L"Canvas Triangle PSO"};
    inline static GraphicsPSO sm_spritePSO{L"Canvas Sprite PSO"};

    //-------------------------------------------------------------------------
    // Dynamic vertex buffers
    //-------------------------------------------------------------------------
    static constexpr size_t MAX_PRIMITIVE_VERTICES = 65536;
    static constexpr size_t MAX_SPRITE_VERTICES = 65536;

    inline static std::vector<VertexPositionColor> sm_lineVertices;
    inline static std::vector<VertexPositionColor> sm_triangleVertices;
    inline static std::vector<VertexPositionTextureColor> sm_spriteVertices;

    inline static com_ptr<ID3D12Resource> sm_lineUploadBuffer;
    inline static com_ptr<ID3D12Resource> sm_triangleUploadBuffer;
    inline static com_ptr<ID3D12Resource> sm_spriteUploadBuffer;
    inline static com_ptr<ID3D12Resource> sm_constantBuffer;

    // Persistently mapped buffer pointers (upload heaps can stay mapped)
    inline static void* sm_lineUploadBufferMapped = nullptr;
    inline static void* sm_triangleUploadBufferMapped = nullptr;
    inline static void* sm_spriteUploadBufferMapped = nullptr;
    inline static void* sm_constantBufferMapped = nullptr;

    //-------------------------------------------------------------------------
    // Sprite batching state
    //-------------------------------------------------------------------------
    inline static int sm_currentTextureIndex = -1;

    //-------------------------------------------------------------------------
    // Clipping stack
    //-------------------------------------------------------------------------
    inline static std::vector<RECT> sm_clipStack;
    inline static RECT sm_currentClipRect = {0, 0, 0, 0};

    //-------------------------------------------------------------------------
    // Layer stack
    //-------------------------------------------------------------------------
    inline static std::vector<float> sm_layerStack;
    inline static float sm_currentZ = 0.0f;

    //-------------------------------------------------------------------------
    // Constant buffer
    //-------------------------------------------------------------------------
    inline static CanvasConstants sm_constants;

    //-------------------------------------------------------------------------
    // Textures (non-owning pointers - owned by TextureManager)
    //-------------------------------------------------------------------------
    inline static std::vector<Texture*> sm_textures;

    //-------------------------------------------------------------------------
    // Fonts (managed by FontPrint)
    //-------------------------------------------------------------------------
    inline static std::vector<std::unique_ptr<FontPrint>> sm_fonts;

    //-------------------------------------------------------------------------
    // Render target (1920x1080 texture for UI compositing)
    //-------------------------------------------------------------------------
    inline static com_ptr<ID3D12Resource> sm_renderTarget;
    inline static D3D12_CPU_DESCRIPTOR_HANDLE sm_renderTargetRTV = {};
    inline static DescriptorHandle sm_renderTargetSRVHandle;
    inline static D3D12_GPU_DESCRIPTOR_HANDLE sm_renderTargetSRV = {};
    inline static D3D12_RESOURCE_STATES sm_renderTargetState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  };

} // namespace Neuron::Graphics
