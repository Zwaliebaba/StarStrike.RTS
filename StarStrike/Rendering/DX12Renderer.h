#pragma once

#include "PipelineState.h"
#include "RootSignature.h"
#include "VertexTypes.h"
#include "Texture2D.h"

namespace Neuron { struct GdiBitmap; }

namespace StarStrike
{
  // Constant buffer for basic rendering (must be 256-byte aligned)
  __declspec(align(256)) struct BasicConstants
  {
    XMFLOAT4X4 WorldViewProj;
  };

  // Sprite definition for rendering
  struct SpriteInfo
  {
    int textureIndex;
    float x, y;
    float width, height;
    float u0, v0, u1, v1;  // UV coordinates
    XMFLOAT4 color;
  };

  // Manages DirectX 12 rendering pipeline for StarStrike
  // Replaces the legacy OpenGL rendering path
  class DX12Renderer
  {
  public:
    static void Startup();
    static void Shutdown();

    // Frame rendering
    static void BeginFrame();
    static void EndFrame();

    // Primitive rendering (batched)
    static void DrawPixel(float x, float y, const XMFLOAT4& color);
    static void DrawLine(float x1, float y1, float x2, float y2, const XMFLOAT4& color);
    static void DrawTriangle(float x1, float y1, float x2, float y2, float x3, float y3, const XMFLOAT4& color);
    static void DrawRectangle(float left, float top, float right, float bottom, const XMFLOAT4& color);
    static void DrawCircle(float cx, float cy, float radius, const XMFLOAT4& color, bool filled = true);

    // Sprite/texture rendering
    static int LoadTexture(const std::wstring& filename);
    static int LoadTextureFromBitmap(const struct Neuron::GdiBitmap* bitmap);
    static void DrawSprite(int textureIndex, float x, float y, float width, float height, const XMFLOAT4& color = {1,1,1,1});
    static void DrawSpriteUV(int textureIndex, float x, float y, float width, float height,
                             float u0, float v0, float u1, float v1, const XMFLOAT4& color = {1,1,1,1});
    static void FlushSprites();

    // Legacy sprite support (maps OpenGL texture indices to DX12 textures)
    static void RegisterLegacySprite(int legacySpriteIndex, const std::wstring& filename, int srcX, int srcY, int size);
    static void DrawLegacySprite(int legacySpriteIndex, float x, float y);
    static bool HasLegacySprite(int legacySpriteIndex);
    static Texture2D* GetTexture(int index);

    // Text rendering
    static void LoadFont(const std::wstring& fontTexture, const std::wstring& fontMask);
    static void DrawText(float x, float y, const char* text, const XMFLOAT4& color, bool large = false);
    static void DrawTextCentered(float centerX, float y, const char* text, const XMFLOAT4& color, bool large = false);

    static void ClearScreen();

    // Flush batched primitives to GPU
    static void FlushLines();
    static void FlushTriangles();

    // Convert legacy palette index to XMFLOAT4 color
    static XMFLOAT4 PaletteToColor(int paletteIndex);

    // Set palette from RGBQUAD array (call after loading scanner bitmap)
    static void SetPaletteFromRGBQUAD(const RGBQUAD* palette, int count);

    // Clip region (scissor rect) - coordinates in game space (0-512)
    static void SetClipRegion(int left, int top, int right, int bottom);
    static void ResetClipRegion();

    // Accessors
    static ID3D12GraphicsCommandList* GetCommandList();
    static const RootSignature& GetBasicRootSignature() { return sm_basicRootSignature; }
    static GraphicsPSO& GetLinePSO() { return sm_linePSO; }
    static GraphicsPSO& GetTrianglePSO() { return sm_trianglePSO; }

  private:
    static void CreateRootSignatures();
    static void CreatePipelineStates();
    static void CreateDynamicBuffers();
    static void UpdateProjectionMatrix();
    static void ApplyClipRegion();

    // Root signatures
    inline static RootSignature sm_basicRootSignature;
    inline static RootSignature sm_spriteRootSignature;

    // Pipeline state objects
    inline static GraphicsPSO sm_linePSO{L"Line PSO"};
    inline static GraphicsPSO sm_trianglePSO{L"Triangle PSO"};
    inline static GraphicsPSO sm_spritePSO{L"Sprite PSO"};

    // Dynamic vertex buffers for batching
    static constexpr size_t MAX_BATCH_VERTICES = 65536;
    inline static std::vector<VertexPositionColor> sm_lineVertices;
    inline static std::vector<VertexPositionColor> sm_triangleVertices;

    // Upload buffers (CPU-writable)
    inline static com_ptr<ID3D12Resource> sm_lineUploadBuffer;
    inline static com_ptr<ID3D12Resource> sm_triangleUploadBuffer;
    inline static com_ptr<ID3D12Resource> sm_constantUploadBuffer;

    // Constant buffer data
    inline static BasicConstants sm_constants;
    inline static void* sm_constantBufferMapped = nullptr;

    // Screen dimensions for projection
    inline static float sm_screenWidth = 512.0f;
    inline static float sm_screenHeight = 512.0f;

    // Clip region (in game coordinates, before scaling)
    inline static int sm_clipLeft = 0;
    inline static int sm_clipTop = 0;
    inline static int sm_clipRight = 512;
    inline static int sm_clipBottom = 512;
    inline static bool sm_clipDirty = true;

    // Legacy palette for color conversion
    inline static std::vector<XMFLOAT4> sm_palette;
    static void LoadPalette();

    // Texture storage
    inline static std::vector<std::unique_ptr<Texture2D>> sm_textures;

    // Mapping from legacy sprite indices (IMG_*) to DX12 texture indices
    inline static std::unordered_map<int, int> sm_legacySpriteMap;

    // Sprite batching
    inline static std::vector<SpriteInfo> sm_sprites;
    inline static std::vector<VertexPositionTextureColor> sm_spriteVertices;
    inline static com_ptr<ID3D12Resource> sm_spriteUploadBuffer;

    // Font rendering
    inline static int sm_fontTextureSmall = -1;
    inline static int sm_fontTextureLarge = -1;
    inline static std::vector<int> sm_charWidthsSmall;
    inline static std::vector<int> sm_charWidthsLarge;
    static void LoadFontMetrics(const struct Neuron::GdiBitmap* mask, std::vector<int>& widths);
  };
}
