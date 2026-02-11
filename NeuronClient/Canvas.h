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

  // Logical/reference resolution for UI authoring (actual rendering uses backbuffer size)
  constexpr float CANVAS_LOGICAL_WIDTH = 1920.0f;
  constexpr float CANVAS_LOGICAL_HEIGHT = 1080.0f;

  // Default font grid parameters (8x8 monospace starting at ASCII 32)
  constexpr uint32_t DEFAULT_GLYPH_WIDTH = 8;
  constexpr uint32_t DEFAULT_GLYPH_HEIGHT = 8;
  constexpr uint32_t DEFAULT_CHARS_PER_ROW = 16;
  constexpr uint32_t DEFAULT_FIRST_CHAR = 32;

  // Aspect ratio handling modes
  enum class AspectMode
  {
    Stretch,      // Stretch to fill (distorts if aspect differs)
    ScaleToFit,   // Uniform scale, letterbox/pillarbox as needed
    ScaleToFill,  // Uniform scale, crops edges to fill
    None          // No scaling, 1:1 pixel mapping from top-left
  };

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
  // - Resolution-independent: uses logical 1920x1080 coordinates, renders directly to backbuffer
  // - Supports multiple aspect ratio handling modes
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
    // Configuration
    //-------------------------------------------------------------------------
    
    /// Configure aspect ratio handling and UI scale
    /// @param _mode How to handle aspect ratio differences
    /// @param _uiScale Additional UI scale multiplier (default 1.0)
    static void Configure(AspectMode _mode, float _uiScale = 1.0f);
    
    /// Called when the backbuffer/window size changes
    static void OnResize(uint32_t _width, uint32_t _height);

    //-------------------------------------------------------------------------
    // Frame control
    //-------------------------------------------------------------------------
    static void BeginFrame();
    static void Render();  // Emits all recorded DX12 commands, renders to backbuffer

    //-------------------------------------------------------------------------
    // Coordinate system
    //-------------------------------------------------------------------------
    
    /// Get logical (authoring) resolution - always 1920x1080
    [[nodiscard]] static float GetLogicalWidth() { return CANVAS_LOGICAL_WIDTH; }
    [[nodiscard]] static float GetLogicalHeight() { return CANVAS_LOGICAL_HEIGHT; }
    
    /// Get physical (backbuffer) resolution
    [[nodiscard]] static uint32_t GetPhysicalWidth() { return sm_physicalWidth; }
    [[nodiscard]] static uint32_t GetPhysicalHeight() { return sm_physicalHeight; }
    
    /// Get DPI scale factor (from system)
    [[nodiscard]] static float GetDpiScale() { return sm_dpiScale; }
    
    /// Get effective UI scale (DPI * user preference)
    [[nodiscard]] static float GetEffectiveScale() { return sm_dpiScale * sm_uiScale; }
    
    /// Get visible logical bounds (accounting for letterboxing/pillarboxing)
    [[nodiscard]] static RECT GetVisibleLogicalRect();
    
    /// Convert logical coordinates to physical (screen) coordinates
    [[nodiscard]] static XMFLOAT2 LogicalToPhysical(float _x, float _y);
    
    /// Convert physical (screen) coordinates to logical coordinates (for input hit-testing)
    [[nodiscard]] static XMFLOAT2 PhysicalToLogical(float _x, float _y);
    
    // Legacy accessors (for backward compatibility)
    [[nodiscard]] static float GetVirtualWidth() { return CANVAS_LOGICAL_WIDTH; }
    [[nodiscard]] static float GetVirtualHeight() { return CANVAS_LOGICAL_HEIGHT; }

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
    // State queries
    //-------------------------------------------------------------------------
    [[nodiscard]] static bool IsValid() { return sm_initialized; }
    [[nodiscard]] static AspectMode GetAspectMode() { return sm_aspectMode; }

  private:
    //-------------------------------------------------------------------------
    // Internal helpers
    //-------------------------------------------------------------------------
    static void CreateRootSignature();
    static void CreatePipelineStates();
    static void CreateDynamicBuffers();
    static void UpdateProjection();
    static void UpdateScaling();

    static void FlushPrimitives();
    static void FlushSprites();
    static void ApplyClipRect(const RECT& _rect);
    static RECT TransformClipRect(const RECT& _logicalRect);

    static void RecordSpriteQuad(float _x, float _y, float _width, float _height,
                                 float _u0, float _v0, float _u1, float _v1, FXMVECTOR _color);

    /// Get GPU address of current frame's constant buffer slice
    [[nodiscard]] static D3D12_GPU_VIRTUAL_ADDRESS GetConstantBufferGPUAddress();

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

    // Per-frame constant buffers (3 frames in flight, 256-byte aligned each)
    static constexpr size_t CONSTANT_BUFFER_FRAME_SIZE = (sizeof(CanvasConstants) + 255) & ~255;
    inline static com_ptr<ID3D12Resource> sm_constantBuffer;
    inline static void* sm_constantBufferMapped = nullptr;

    //-------------------------------------------------------------------------
    // Sprite batching state
    //-------------------------------------------------------------------------
    inline static int sm_currentTextureIndex = -1;

    //-------------------------------------------------------------------------
    // Resolution and scaling state
    //-------------------------------------------------------------------------
    inline static uint32_t sm_physicalWidth = 1920;
    inline static uint32_t sm_physicalHeight = 1080;
    inline static float sm_dpiScale = 1.0f;
    inline static float sm_uiScale = 1.0f;
    inline static AspectMode sm_aspectMode = AspectMode::ScaleToFit;
    inline static bool sm_initialized = false;
    
    // Computed scaling values (updated by UpdateScaling)
    inline static float sm_scaleX = 1.0f;          // Logical to physical X scale
    inline static float sm_scaleY = 1.0f;          // Logical to physical Y scale
    inline static float sm_offsetX = 0.0f;         // Physical X offset (for letterboxing)
    inline static float sm_offsetY = 0.0f;         // Physical Y offset (for pillarboxing)
    inline static float sm_uniformScale = 1.0f;    // Uniform scale for ScaleToFit/ScaleToFill

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
  };

} // namespace Neuron::Graphics
