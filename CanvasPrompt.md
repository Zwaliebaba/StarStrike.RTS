# Prompt: Enhance Canvas System for GUI Windows in StarStrike

## Goal

Create a Canvas rendering system for StarStrike that supports retro-style GUI windows rendered as a 2D overlay on top of the 3D scene. The system should render bitmap-font text, draggable/resizable windows with title bars, scrollable content areas, labeled rows with dropdown controls, and real-time graph/bar visualizations — all using DirectX 12, matching the visual style shown in the reference screenshots (dark semi-transparent panels, light-blue beveled window borders, monospaced bitmap fonts).

## Context & Existing Infrastructure

### Engine architecture (Neuron)

- Static singleton pattern: classes use `inline static` members, `Startup()`/`Shutdown()` lifecycle
- Namespace: all engine code under `Neuron::` (e.g., `Neuron::Graphics::Core`)
- Naming: parameters `_prefixed`, members `m_`, statics `sm_`, PascalCase classes/methods, camelCase locals
- C++20, DirectXMath types, `winrt::com_ptr<T>` for COM objects
- `DebugTrace("...\n")` for debug output (always end with `\n`)
- **PCH**: All `.cpp` files in StarStrike must `#include "pch.h"` as first include. `pch.h` includes `NeuronClient.h` which transitively provides all engine headers (`GraphicsCore.h`, `Texture.h`, `TextureManager.h`, etc.)

### Graphics pipeline available

- `Graphics::Core::GetD3DDevice()` → `ID3D12Device10*`
- `Graphics::Core::GetCommandList()` → `ID3D12GraphicsCommandList7*`
- `Graphics::Core::GetBackBufferFormat()`, `GetDepthBufferFormat()`
- `Graphics::Core::GetScreenViewport()`, `GetScissorRect()`, `GetOutputSize()`
- `Graphics::Core::GetCurrentFrameIndex()` → `UINT` (current back buffer index, for per-frame resource management)
- `Graphics::Core::GetBackBufferCount()` → `UINT` (number of swap chain buffers, typically 2–3)
- `Graphics::Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)` → `DescriptorHandle` (shader-visible by default for CBV/SRV/UAV types)
- `DescriptorAllocator::SetDescriptorHeaps(cmdList)` — **must be called** before `SetGraphicsRootDescriptorTable()` to bind the shader-visible SRV/Sampler heaps
- Common states: `Graphics::BlendTraditional` (SrcA, 1-SrcA), `Graphics::BlendPreMultiplied`, `Graphics::DepthStateDisabled`, `Graphics::RasterizerDefault`, `Graphics::RasterizerTwoSided`
- `Graphics::SamplerPointClampDesc` — `SamplerDesc` with point filtering + clamp addressing (already initialized in `GraphicsCommon.cpp`), suitable for static sampler via `RootSignature::InitStaticSampler()`
- `RootSignature` and `GraphicsPSO` classes for pipeline setup
- `TextureManager::LoadFromFile(L"Fonts/EditorFont-ENG.dds")` returns `Texture*` with `.GetSRV()` (returns `DescriptorHandle`, implicitly converts to `D3D12_GPU_DESCRIPTOR_HANDLE`), `.GetWidth()`, `.GetHeight()`

### DescriptorHandle type note

`Texture::GetSRV()` returns `DescriptorHandle`, not raw `D3D12_GPU_DESCRIPTOR_HANDLE`. `DescriptorHandle` has implicit conversion operators to both `D3D12_CPU_DESCRIPTOR_HANDLE` and `D3D12_GPU_DESCRIPTOR_HANDLE`. When comparing handles for batch-break decisions, compare `.GetGpuPtr()` (`uint64_t`) since `D3D12_GPU_DESCRIPTOR_HANDLE` has no `operator==`.

### Vertex types already defined (in `NeuronClient/VertexTypes.h`)

- `VertexPositionColor` — `XMFLOAT3 m_position`, `XMFLOAT4 m_color` (28 bytes)
- `VertexPositionTexture` — `XMFLOAT3 m_position`, `XMFLOAT2 m_texcoord` (20 bytes)
- `VertexPositionTextureColor` — `XMFLOAT3 m_position`, `XMFLOAT2 m_texcoord`, `XMFLOAT4 m_color` (36 bytes) — **use this for the Canvas** since we need position + UV + tint color

### Existing shaders

- `WorldVS.hlsl` / `WorldPS.hlsl` — 3D world rendering (position+color, uses `WorldViewProj` matrix)
- Pre-compiled `SpriteVS.h` / `SpritePS.h` exist in `NeuronClient/CompiledShaders/` and `StarStrike/CompiledShaders/` with the same POSITION+TEXCOORD+COLOR layout, but use a full 4x4 `WorldViewProj` matrix — **not reused** by the Canvas; we write new lightweight shaders with only 2 root constants
- Compiled shader headers go to `StarStrike/CompiledShaders/` as `g_p%(Filename)` byte arrays
- Shaders are compiled externally via `dxc` and the resulting `.h` bytecode arrays are committed to source control

### Font atlas textures (DDS)

- `Assets/Fonts/EditorFont-ENG.dds` — fixed-width 8×8 pixel font (for UI labels, menus)
- `Assets/Fonts/SpeccyFont-ENG.dds` — fixed-width 8×8 pixel font (for profiler/debug data, monospaced display)
- Both are 256-character bitmap atlas textures arranged in a 16×16 grid (each cell is one character, ASCII order starting from space at position 0x20)
- Characters map to ASCII: glyph index = `charCode - 32`. UV for a character at grid position `(col, row)` where `col = index % 16`, `row = index / 16`. Each cell is `1/16` of the texture in both U and V.

### Game integration point

- `GameApp` extends `GameMain` which has a `Render()` override
- Currently `GameApp::Render()` clears the screen, sets render targets/viewport/scissor, and calls `m_worldRenderer.Render(...)`
- **Note**: `GameMain` does NOT have a separate `RenderCanvas()` virtual — all rendering goes through `Render()`. Canvas rendering is appended at the end of `GameApp::Render()`.
- The Canvas should be rendered **after** the 3D world, as a 2D overlay (depth testing disabled, alpha blending enabled)

## What to Build

### 1. Canvas Shaders (`StarStrike/Shaders/`)

**`CanvasVS.hlsl`:**
- Vertex input: `float3 position : POSITION`, `float2 texcoord : TEXCOORD`, `float4 color : COLOR`
- Root constant buffer at `b0`: `float2 InvViewportSize` (1/width, 1/height)
- Transform pixel coordinates to NDC: `x_ndc = position.x * InvViewportSize.x * 2 - 1`, `y_ndc = 1 - position.y * InvViewportSize.y * 2`
- Pass through texcoord and color to pixel shader
- Output `float4 position : SV_Position`, `float2 texcoord : TEXCOORD`, `float4 color : COLOR`

**`CanvasPS.hlsl`:**
- Sample font texture `t0` with `SamplerState s0 : register(s0)` (point filtering for crisp pixel fonts)
- Output: `textureSample * input.color` — the vertex color tints the text/glyph
- For solid-colored rectangles (non-textured), use a 1×1 white pixel region of the atlas (or UV = 0,0 if the top-left is white), so the color pass-through works for both textured glyphs and flat quads

### 2. BitmapFont Class (`StarStrike/BitmapFont.h` / `.cpp`)

```cpp
namespace Neuron
{
  class BitmapFont
  {
  public:
    void Load(const std::wstring& _atlasPath, uint32_t _cellWidth, uint32_t _cellHeight,
              uint32_t _gridCols = 16, uint32_t _gridRows = 16);

    // Measures string width in pixels
    uint32_t MeasureString(std::string_view _text) const;

    // Returns cell dimensions
    uint32_t GetCellWidth() const;
    uint32_t GetCellHeight() const;
    uint32_t GetLineHeight() const; // cellHeight + 2px spacing

    // Gets UV rect for a character (returns min/max UV as XMFLOAT4: u0,v0,u1,v1)
    XMFLOAT4 GetGlyphUV(char _ch) const;

    // Get texture SRV for binding (returns DescriptorHandle, converts to D3D12_GPU_DESCRIPTOR_HANDLE)
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const;

  private:
    Texture* m_texture = nullptr;
    uint32_t m_cellWidth = 8;
    uint32_t m_cellHeight = 8;
    uint32_t m_gridCols = 16;
    uint32_t m_gridRows = 16;
  };
}
```

- Glyph index: `max(0, charCode - 32)`, clamped to `gridCols * gridRows - 1`
- UV calculation: `col = index % gridCols`, `row = index / gridCols`, `u0 = col / gridCols`, `v0 = row / gridRows`, etc.
- UV for solid white: provide a `GetWhiteUV()` method that returns the UV rect for a known-white region (e.g., glyph index 0 = space character). If the font atlas space glyph isn't solid white, the Canvas must create a 1×1 white texture instead.

### 3. Canvas Class (`StarStrike/Canvas.h` / `.cpp`)

The Canvas is an immediate-mode 2D rendering system that batches textured/colored quads.

```cpp
namespace Neuron
{
  class Canvas
  {
  public:
    void Startup();
    void Shutdown();

    // Call at start of frame's UI pass
    void Begin();

    // Drawing primitives (all coordinates in screen pixels, origin top-left)
    void DrawRect(float _x, float _y, float _w, float _h, const XMFLOAT4& _color);
    void DrawRectOutline(float _x, float _y, float _w, float _h, float _thickness,
                         const XMFLOAT4& _color);
    void DrawText(BitmapFont& _font, float _x, float _y, std::string_view _text,
                  const XMFLOAT4& _color);
    void DrawTextClipped(BitmapFont& _font, float _x, float _y, float _maxWidth,
                         std::string_view _text, const XMFLOAT4& _color);

    // Flush all batched quads to GPU
    void End();

  private:
    void FlushBatch();
    void AddQuad(float _x, float _y, float _w, float _h, const XMFLOAT4& _uv,
                 const XMFLOAT4& _color, D3D12_GPU_DESCRIPTOR_HANDLE _srv);

    RootSignature m_rootSig;
    GraphicsPSO   m_pso;

    // Dynamic vertex buffer (uploaded each frame)
    static constexpr uint32_t MAX_QUADS = 4096;
    static constexpr uint32_t MAX_VERTICES = MAX_QUADS * 6; // 6 verts per quad (two triangles)
    std::vector<VertexPositionTextureColor> m_vertices;
    D3D12_GPU_DESCRIPTOR_HANDLE m_currentTexture = {};

    // Per-frame upload buffers to avoid GPU/CPU contention
    // Indexed by Graphics::Core::GetCurrentFrameIndex()
    static constexpr uint32_t MAX_FRAME_COUNT = 3;
    com_ptr<ID3D12Resource> m_uploadBuffers[MAX_FRAME_COUNT];
    void* m_mappedBuffers[MAX_FRAME_COUNT] = {};

    // White texture SRV for solid-color rects
    D3D12_GPU_DESCRIPTOR_HANDLE m_whiteSRV = {};
    com_ptr<ID3D12Resource> m_whiteTexture;
    com_ptr<ID3D12Resource> m_whiteUpload;
  };
}
```

**Root signature layout:**
- Slot 0: Root constants — 2 floats (`InvViewportSize`) at `b0`, visibility `D3D12_SHADER_VISIBILITY_VERTEX`
- Slot 1: Descriptor table — 1 SRV range at `t0` (the font/texture atlas), visibility `D3D12_SHADER_VISIBILITY_PIXEL`
- Static sampler at `s0`: Use `Graphics::SamplerPointClampDesc` via `m_rootSig.InitStaticSampler(0, SamplerPointClampDesc, D3D12_SHADER_VISIBILITY_PIXEL)` — point filter, clamp addressing for crisp pixel fonts

```cpp
// Root signature setup in Canvas::Startup()
m_rootSig.Reset(2, 1); // 2 root params, 1 static sampler
m_rootSig[0].InitAsConstants(0, 2, D3D12_SHADER_VISIBILITY_VERTEX); // b0: 2 floats
m_rootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1,
                                    D3D12_SHADER_VISIBILITY_PIXEL); // t0: 1 SRV
m_rootSig.InitStaticSampler(0, Graphics::SamplerPointClampDesc, D3D12_SHADER_VISIBILITY_PIXEL);
m_rootSig.Finalize(L"CanvasRootSig",
  D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
  D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
  D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
  D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);
```

**PSO configuration:**
- Input layout: `VertexPositionTextureColor::INPUT_LAYOUT`
- Blend: `Graphics::BlendTraditional` (alpha blending)
- Depth: `Graphics::DepthStateDisabled`
- Rasterizer: `Graphics::RasterizerTwoSided`
- Topology: `D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE`
- Render target format: `Graphics::Core::GetBackBufferFormat()`, depth format `DXGI_FORMAT_UNKNOWN`

```cpp
// PSO setup in Canvas::Startup()
m_pso = GraphicsPSO(L"CanvasPSO");
m_pso.SetRootSignature(m_rootSig);
m_pso.SetVertexShader(g_pCanvasVS, sizeof(g_pCanvasVS));
m_pso.SetPixelShader(g_pCanvasPS, sizeof(g_pCanvasPS));
m_pso.SetInputLayout(&VertexPositionTextureColor::INPUT_LAYOUT);
m_pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
m_pso.SetRenderTargetFormat(Graphics::Core::GetBackBufferFormat(), DXGI_FORMAT_UNKNOWN);
m_pso.SetRasterizerState(Graphics::RasterizerTwoSided);
m_pso.SetBlendState(Graphics::BlendTraditional);
m_pso.SetDepthStencilState(Graphics::DepthStateDisabled);
m_pso.Finalize();
```

**Dynamic upload buffer strategy (per-frame):**

The Canvas needs a dynamic vertex buffer re-uploaded every frame. Unlike `WorldRenderer::CreateUploadedMesh()` which does a blocking default-heap copy, the Canvas uses **upload-heap-only** buffers that are persistently mapped.

To avoid overwriting data the GPU is still reading from a previous frame, maintain **one upload buffer per back buffer** (indexed by `Graphics::Core::GetCurrentFrameIndex()`). The engine's swap chain fence synchronization guarantees that by the time a given frame index is reused, the GPU has finished reading from that frame's buffer.

```cpp
// In Canvas::Startup() — create per-frame upload buffers
auto* device = Graphics::Core::GetD3DDevice();
const UINT bufferSize = MAX_VERTICES * sizeof(VertexPositionTextureColor);
auto uploadProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

for (UINT i = 0; i < Graphics::Core::GetBackBufferCount(); ++i)
{
  check_hresult(device->CreateCommittedResource(
    &uploadProps, D3D12_HEAP_FLAG_NONE, &resDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
    IID_PPV_ARGS(m_uploadBuffers[i].put())));

  // Persistently map — never unmap until Shutdown()
  check_hresult(m_uploadBuffers[i]->Map(0, nullptr, &m_mappedBuffers[i]));
}
```

```cpp
// In Canvas::FlushBatch()
UINT frameIndex = Graphics::Core::GetCurrentFrameIndex();
const UINT vertexCount = static_cast<UINT>(m_vertices.size());
const UINT uploadSize = vertexCount * sizeof(VertexPositionTextureColor);

memcpy(m_mappedBuffers[frameIndex], m_vertices.data(), uploadSize);

D3D12_VERTEX_BUFFER_VIEW vbView = {};
vbView.BufferLocation = m_uploadBuffers[frameIndex]->GetGPUVirtualAddress();
vbView.SizeInBytes = uploadSize;
vbView.StrideInBytes = sizeof(VertexPositionTextureColor);

auto* cmdList = Graphics::Core::GetCommandList();
cmdList->SetPipelineState(m_pso.GetPipelineStateObject());
cmdList->SetGraphicsRootSignature(m_rootSig.GetSignature());

// Set InvViewportSize root constants
auto viewport = Graphics::Core::GetScreenViewport();
float invViewport[2] = { 1.0f / viewport.Width, 1.0f / viewport.Height };
cmdList->SetGraphicsRoot32BitConstants(0, 2, invViewport, 0);

// Bind descriptor heaps (required before SetGraphicsRootDescriptorTable)
DescriptorAllocator::SetDescriptorHeaps(cmdList);
cmdList->SetGraphicsRootDescriptorTable(1, m_currentTexture);

cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
cmdList->IASetVertexBuffers(0, 1, &vbView);
cmdList->DrawInstanced(vertexCount, 1, 0, 0);

m_vertices.clear();
```

**White texture creation (for solid-color rects):**

Create a 1×1 white RGBA texture on the default heap with SRV during `Startup()`:

```cpp
// In Canvas::Startup() — create 1x1 white texture
auto srvHandle = Graphics::Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
m_whiteSRV = static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(srvHandle);

// Create 1x1 default-heap texture
D3D12_RESOURCE_DESC texDesc = {};
texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
texDesc.Width = 1;
texDesc.Height = 1;
texDesc.DepthOrArraySize = 1;
texDesc.MipLevels = 1;
texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
texDesc.SampleDesc.Count = 1;
texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

auto defaultProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
check_hresult(device->CreateCommittedResource(
  &defaultProps, D3D12_HEAP_FLAG_NONE, &texDesc,
  D3D12_RESOURCE_STATE_COMMON, nullptr,
  IID_PPV_ARGS(m_whiteTexture.put())));

// Upload white pixel
uint32_t whitePixel = 0xFFFFFFFF;
// ... (upload via staging buffer, transition, copy, transition — same pattern as WorldRenderer)

// Create SRV
D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
srvDesc.Texture2D.MipLevels = 1;
device->CreateShaderResourceView(m_whiteTexture.get(), &srvDesc, srvHandle);
```

**Batch-break texture comparison:**
```cpp
// In Canvas::AddQuad() — compare by GPU pointer value
if (m_currentTexture.ptr != _srv.ptr && !m_vertices.empty())
  FlushBatch();
m_currentTexture = _srv;
```

### 4. CanvasWindow Class (`StarStrike/CanvasWindow.h` / `.cpp`)

A higher-level composable GUI window built on Canvas primitives.

```cpp
namespace Neuron
{
  class CanvasWindow
  {
  public:
    CanvasWindow(std::string _title, float _x, float _y, float _w, float _h);

    // Configuration
    void SetVisible(bool _visible);
    void SetDraggable(bool _draggable);
    void SetResizable(bool _resizable);

    // Content building (immediate mode - call each frame)
    void BeginWindow(Canvas& _canvas, BitmapFont& _font);
    void LabelRow(const std::string& _label, const std::string& _value);
    void DropdownRow(const std::string& _label, const std::string& _currentValue);
    void Separator();
    void TextLine(const std::string& _text, const XMFLOAT4& _color = {1,1,1,1});
    void ProgressBar(float _fraction, const XMFLOAT4& _fillColor);
    void GraphLine(const std::vector<float>& _values, float _maxValue, const XMFLOAT4& _color);
    void EndWindow();

    // Input handling
    bool HandleMouseDown(float _mx, float _my);
    void HandleMouseMove(float _mx, float _my);
    void HandleMouseUp();

  private:
    std::string m_title;
    float m_x, m_y, m_width, m_height;
    float m_cursorY = 0; // current layout Y position within window content
    bool m_visible = true;
    bool m_draggable = true;
    bool m_resizable = false;
    bool m_dragging = false;
    float m_dragOffsetX = 0, m_dragOffsetY = 0;

    Canvas* m_canvas = nullptr;
    BitmapFont* m_font = nullptr;
  };
}
```

**Visual style (matching reference screenshots):**
- **Title bar**: Dark background (`0.15, 0.15, 0.2, 0.95`), centered title text in uppercase, light blue color (`0.6, 0.75, 0.9, 1.0`)
- **Window body**: Very dark semi-transparent fill (`0.08, 0.08, 0.12, 0.9`)
- **Border**: 1px light blue/grey outline (`0.4, 0.5, 0.6, 0.8`) — drawn with `DrawRectOutline`
- **Label rows**: Label left-aligned in dim white, value right-aligned or followed by dropdown arrow `▼`
- **Title bar height**: `font.GetLineHeight() + 8` pixels
- **Row height**: `font.GetLineHeight() + 4` pixels
- **Padding**: 8px horizontal, 4px vertical between rows

**Input handling note:** `GameMain` does not provide mouse event hooks. `GameApp` currently uses `GetAsyncKeyState()` for input. CanvasWindow mouse interaction requires similar polling via `GetCursorPos()` / `ScreenToClient()` in `GameApp::Update()`, forwarding to `HandleMouseDown`/`HandleMouseMove`/`HandleMouseUp`. This can be wired up incrementally — the initial implementation should focus on rendering correctness, with drag/resize input as a follow-up.

### 5. Integration into GameApp

Add to `GameApp.h`:

```cpp
#include "Canvas.h"
#include "CanvasWindow.h"
#include "BitmapFont.h"

// New members:
Canvas      m_canvas;
BitmapFont  m_editorFont;
BitmapFont  m_monoFont;
// Example window:
std::unique_ptr<CanvasWindow> m_debugWindow;
```

In `GameApp::Startup()`:

```cpp
m_canvas.Startup();
m_editorFont.Load(L"Fonts/EditorFont-ENG.dds", 8, 8);
m_monoFont.Load(L"Fonts/SpeccyFont-ENG.dds", 8, 8);
m_debugWindow = std::make_unique<CanvasWindow>("PROFILER", 800, 30, 400, 200);
```

In `GameApp::Render()` at the end (after world rendering):

```cpp
// --- 2D Canvas Overlay ---
m_canvas.Begin();
m_debugWindow->BeginWindow(m_canvas, m_monoFont);
float deltaMs = Timer::Core::GetElapsedSeconds() * 1000.0f;
int fps = static_cast<int>(1.0f / Timer::Core::GetElapsedSeconds());
m_debugWindow->TextLine(std::format("{:.2f} ms ({} fps)", deltaMs, fps));
m_debugWindow->EndWindow();
m_canvas.End();
```

**Note on `std::format`**: The vcxproj sets `stdcpp17` for VS < 18.0 and `stdcpplatest` for VS 18+. `std::format` requires C++20. If building with an older toolchain, use `sprintf_s` or a small formatting helper instead.

In `GameApp::Shutdown()`:

```cpp
m_debugWindow.reset();
m_canvas.Shutdown();
```

## Shader Build Configuration

Shaders are compiled externally via `dxc` (matching the existing pattern — the project does NOT use `<FxCompile>` in the vcxproj). The compiled bytecode header files are committed to source control.

**Compile commands:**

```bash
# From StarStrike/Shaders/ directory
dxc -T vs_6_7 -E main -Fh ../CompiledShaders/CanvasVS.h -Vn g_pCanvasVS CanvasVS.hlsl
dxc -T ps_6_7 -E main -Fh ../CompiledShaders/CanvasPS.h -Vn g_pCanvasPS CanvasPS.hlsl
```

This produces `StarStrike/CompiledShaders/CanvasVS.h` and `StarStrike/CompiledShaders/CanvasPS.h` containing `const unsigned char g_pCanvasVS[]` and `g_pCanvasPS[]` byte arrays, matching the existing `BasicVS.h`/`BasicPS.h` pattern.

The `.hlsl` source files are added to the vcxproj as `<None>` items (not `<FxCompile>`), consistent with how `WorldVS.hlsl`/`WorldPS.hlsl` are listed:

```xml
<ItemGroup>
  <None Include="Shaders\WorldPS.hlsl" />
  <None Include="Shaders\WorldVS.hlsl" />
  <None Include="Shaders\CanvasVS.hlsl" />
  <None Include="Shaders\CanvasPS.hlsl" />
</ItemGroup>
```

## Runtime Draw-Call Checklist

Each `FlushBatch()` must perform these steps in order:

1. `cmdList->SetPipelineState(m_pso.GetPipelineStateObject())`
2. `cmdList->SetGraphicsRootSignature(m_rootSig.GetSignature())`
3. `cmdList->SetGraphicsRoot32BitConstants(0, 2, invViewport, 0)` — InvViewportSize
4. `DescriptorAllocator::SetDescriptorHeaps(cmdList)` — **required** before descriptor table binding
5. `cmdList->SetGraphicsRootDescriptorTable(1, m_currentTexture)` — bind font atlas SRV
6. `cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)` — runtime topology (distinct from PSO's topology *type*)
7. `cmdList->IASetVertexBuffers(0, 1, &vbView)`
8. `cmdList->DrawInstanced(vertexCount, 1, 0, 0)`

## File Summary

| File | Project | Purpose |
|------|---------|---------|
| `StarStrike/Shaders/CanvasVS.hlsl` | StarStrike | 2D canvas vertex shader (pixel→NDC transform) |
| `StarStrike/Shaders/CanvasPS.hlsl` | StarStrike | 2D canvas pixel shader (texture × color) |
| `StarStrike/CompiledShaders/CanvasVS.h` | StarStrike | Pre-compiled VS bytecode (committed, generated by `dxc`) |
| `StarStrike/CompiledShaders/CanvasPS.h` | StarStrike | Pre-compiled PS bytecode (committed, generated by `dxc`) |
| `StarStrike/BitmapFont.h` / `.cpp` | StarStrike | Bitmap font atlas loader + glyph UV lookup |
| `StarStrike/Canvas.h` / `.cpp` | StarStrike | Low-level batched 2D quad renderer |
| `StarStrike/CanvasWindow.h` / `.cpp` | StarStrike | High-level GUI window compositing |
| `StarStrike/GameApp.h` / `.cpp` | StarStrike | Integration (modified, add canvas + fonts + windows) |

## Key Constraints

- Use `VertexPositionTextureColor` — it already exists in the engine
- Use `TextureManager::LoadFromFile()` for loading DDS font atlases
- Use `Graphics::Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)` for creating the white texture SRV
- Call `DescriptorAllocator::SetDescriptorHeaps(cmdList)` before any `SetGraphicsRootDescriptorTable()` call
- Call `cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)` at draw time
- All new classes in `namespace Neuron`
- All new `.cpp` files must `#include "pch.h"` as the first include (precompiled header)
- No depth testing for Canvas — it's a screen-space overlay (`DXGI_FORMAT_UNKNOWN` for DSV format in PSO)
- Alpha blending enabled (`BlendTraditional`)
- Point-filtered sampler for crisp pixel fonts (no bilinear blur) — use `SamplerPointClampDesc` as static sampler
- Per-frame upload buffers indexed by `Graphics::Core::GetCurrentFrameIndex()`, persistently mapped, one per back buffer to avoid GPU/CPU data races
- Compare texture handles by `.ptr` value for batch breaks
- Coordinates: screen pixels, origin at top-left, Y increases downward
- Shaders compiled via `dxc` externally; `.hlsl` listed as `<None>` in vcxproj; `.h` bytecode committed to source control
