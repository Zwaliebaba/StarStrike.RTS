# Copilot Instructions for StarStrike.RTS

## Project Overview

StarStrike is a DirectX 12 space game built on the Neuron engine framework. Three main projects:

- **NeuronCore** (`NeuronCore/`): Foundation utilities - math, file system, timers, debugging
- **NeuronClient** (`NeuronClient/`): DirectX 12 graphics engine, audio (XAudio2), input handling
- **StarStrike** (`StarStrike/`): Game implementation extending `GameMain`

## Architecture Patterns

### Static Singleton Pattern
Core engine systems use static class methods with `inline static` members (no instance singletons):
```cpp
class Core {
public:
    static void Startup();
    static void Shutdown();
    static ID3D12Device10* GetD3DDevice() noexcept { return m_d3dDevice.get(); }
private:
    inline static com_ptr<ID3D12Device10> m_d3dDevice;
};
```

### Namespace Hierarchy
All engine code lives under `Neuron::` namespaces:
- `Neuron::Graphics::Core` - D3D12 device, swap chain, command lists
- `Neuron::Audio::Core` - XAudio2 music/sound engines
- `Neuron::Timer::Core` - Frame timing (`GetElapsedSeconds()`, `GetTotalSeconds()`)
- `Neuron::Client::ClientEngine` - Application startup/run loop

### GameMain Interface
Games extend `GameMain` (see [GameMain.h](../NeuronClient/GameMain.h)):
```cpp
class StarStrike : public GameMain {
    void Startup() override;              // Initialize game state
    void Shutdown() override;
    void Update(float _deltaT) override;  // Game logic per frame
    void RenderScene() override;          // 3D rendering
    void RenderCanvas() override;         // UI overlay (LDR)
};
```

### Application Startup Flow (WinMain.cpp)
```cpp
FileSys::SetHomeDirectory(exePath);           // Sets asset root to {exe}\Assets\
ClientEngine::Startup(L"GameName", {}, hInstance, nCmdShow);
auto main = winrt::make_self<StarStrike>();   // Create game via WinRT
ClientEngine::StartGame(std::move(main));
ClientEngine::Run();                          // Main loop
ClientEngine::Shutdown();
```

## Coding Conventions

### Naming
- **Parameters**: Underscore prefix: `_deltaT`, `_size`, `_hInstance`
- **Instance members**: `m_` prefix
- **Static members**: `sm_` prefix
- **Classes/Methods**: PascalCase
- **Local variables**: camelCase

### Legacy Code Warning
Files like `elite.h`, `space.h`, `trade.h` contain ported C-style code. **Always use modern C++ patterns for new code** - do not match legacy style.

### Modern C++
- C++20 (`std::format`, `std::ranges`, `[[nodiscard]]`)
- DirectXMath types (`XMVECTOR`, `XMMATRIX`, `XMFLOAT3`, `XMFLOAT4`)
- `winrt::com_ptr<T>` for COM objects

## Build & Dependencies

### Build System
- Solution: `StarStrike.slnx`
- Platform: **x64 only**
- Run clang-format before committing (`.clang-format` in root)

### Package Management
- **NuGet**: Windows App SDK, CppWinRT, PIX profiler (see `packages.config`)
- vcpkg is **disabled** (`VcpkgEnableManifest=false`)

### Shader Workflow
1. Add `.hlsl` files to `StarStrike/Shaders/`
2. Configure in vcxproj: `FxCompile` with ShaderModel 6.7
3. Build outputs `CompiledShaders/%(Filename).h` with bytecode array `g_p%(Filename)`
4. Include and use:
```cpp
#include "CompiledShaders/BasicVS.h"
pso.SetVertexShader(g_pBasicVS, sizeof(g_pBasicVS));
```

### Asset Loading
Assets resolve from `{exe_path}\Assets\` via `FileSys::GetHomeDirectory()`:
```cpp
auto data = BinaryFile::ReadFile(L"scanner.bmp");
auto text = TextFile::ReadFile(L"config.txt");
```

## Key Integration Points

### Graphics Pipeline
1. Get command list: `Graphics::Core::GetCommandList()`
2. Transition resources: `Graphics::Core::GetGpuResourceStateTracker()->TransitionResource(...)`
3. Allocate descriptors: `Graphics::Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)`

### Vertex Types (VertexTypes.h)
Pre-defined vertex structs with `INPUT_LAYOUT` for PSO setup:
- `VertexPosition` - Position only
- `VertexPositionColor` - Position + RGBA color
- `VertexPositionTexture` - Position + UV
- `VertexPositionTextureColor` - Position + UV + color

### Creating Pipeline States
```cpp
GraphicsPSO pso(L"MyPSO");
pso.SetRootSignature(rootSig);
pso.SetVertexShader(g_pMyVS, sizeof(g_pMyVS));
pso.SetPixelShader(g_pMyPS, sizeof(g_pMyPS));
pso.SetInputLayout(&VertexPositionColor::INPUT_LAYOUT);
pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
pso.Finalize();
```

### Constant Buffers
Constant buffers must be 256-byte aligned:
```cpp
__declspec(align(256)) struct MyConstants {
    XMFLOAT4X4 WorldViewProj;
};
```

### 2D Rendering (DX12Renderer)
Use `StarStrike::DX12Renderer` for batched 2D primitives and sprites:
```cpp
DX12Renderer::BeginFrame();
DX12Renderer::DrawLine(x1, y1, x2, y2, color);
DX12Renderer::DrawRectangle(left, top, right, bottom, color);
DX12Renderer::DrawSprite(textureIndex, x, y, width, height);
DX12Renderer::DrawText(x, y, "text", color);
DX12Renderer::EndFrame();  // Flushes all batched primitives
```

### Audio
Separate engines for music vs sound effects:
```cpp
// Load and play music (loops by default)
MusicTrack music;
music.Load(Audio::Core::MusicEngine(), L"theme.wav");
music.Play(true);  // true = loop

// Load and play sound effects
SoundEffect sfx;
sfx.Load(Audio::Core::SoundEffectEngine(), L"laser.wav");
sfx.Play();  // one-shot
```

### Debug Utilities
**Always end DebugTrace strings with `\n`:**
```cpp
DebugTrace("Value: {}\n", value);  // Debug output (release: no-op)
ASSERT(condition);                          // Fatal in all builds
DEBUG_ASSERT(condition);                    // Debug-only assertion
DEBUG_ASSERT_TEXT(condition, "Condition failed: {}\n", details);  // Debug-only with message
```

### Testing
No testing infrastructure available.
