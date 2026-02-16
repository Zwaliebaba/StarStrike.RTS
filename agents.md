# Agent Guidance for StarStrike.RTS

## Mission
- Maintain and develop StarStrike, a DirectX 12 space game built on the Neuron engine framework.
- Follow established architecture patterns using the Neuron engine layer.
- Prioritize modern C++20 practices and DirectX 12 best practices.

## Project Structure
- **NeuronCore** (`NeuronCore/`): Foundation utilities (math, file system, timers, debugging)
- **NeuronClient** (`NeuronClient/`): DirectX 12 graphics engine, audio (XAudio2), input handling, and game framework
- **StarStrike** (`StarStrike/`): Game implementation and main executable
- **GameLogic** (`GameLogic/`): Placeholder for future game logic library
- **NeuronServer** (`NeuronServer/`): Placeholder for potential server implementation
- **Server** (`Server/`): Standalone server project (currently minimal)

## Key Architecture Patterns

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
Games extend `GameMain` from NeuronClient:
```cpp
class YourGame : public GameMain {
    void Startup() override;              // Initialize game state
    void Shutdown() override;              // Cleanup
    void Update(float _deltaT) override;  // Game logic per frame
    void RenderScene() override;          // 3D rendering
    void RenderCanvas() override;         // UI overlay (LDR)
};
```

### Application Startup Flow (WinMain.cpp)
```cpp
FileSys::SetHomeDirectory(exePath);           // Sets asset root to {exe}\Assets\
ClientEngine::Startup(L"GameName", {}, hInstance, nCmdShow);
// Optional: Create and start game
// auto main = winrt::make_self<YourGame>();
// ClientEngine::StartGame(std::move(main));
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

### Modern C++
- C++20 (`std::format`, `std::ranges`, `[[nodiscard]]`)
- DirectXMath types (`XMVECTOR`, `XMMATRIX`, `XMFLOAT3`, `XMFLOAT4`)
- `winrt::com_ptr<T>` for COM objects
- Smart pointers and RAII principles

### Debug Utilities
**Always end DebugTrace strings with `\n`:**
```cpp
DebugTrace("Value: {}\n", value);      // Debug output (release: no-op)
ASSERT(condition);                      // Fatal in all builds
DEBUG_ASSERT(condition);                // Debug-only assertion
DEBUG_ASSERT_TEXT(condition, "{}", msg); // Debug-only with message
```

## Build & Dependencies

### Build System
- Solution: `StarStrike.slnx` (recommended for Visual Studio)
- Platform: **x64 only**
- Run clang-format before committing (`.clang-format` in root)

### Package Management
- **NuGet**: Windows App SDK, CppWinRT (see `packages.config`)
- vcpkg is **disabled** (`VcpkgEnableManifest=false`)

## Key Integration Points

### Graphics Pipeline
1. Get device: `Neuron::Graphics::Core::GetD3DDevice()`
2. Get command list: `Neuron::Graphics::Core::GetCommandList()`
3. Allocate descriptors: `Neuron::Graphics::Core::AllocateDescriptor()`
4. Track resource states: `Neuron::Graphics::Core::GetGpuResourceStateTracker()->TransitionResource(...)`

### Audio System
```cpp
// Access audio engines
IXAudio2* musicEngine = Neuron::Audio::Core::MusicEngine();
IXAudio2* sfxEngine = Neuron::Audio::Core::SoundEffectEngine();

// Use SoundEffect class for sound playback
SoundEffect sfx;
sfx.Load(sfxEngine, L"sound.wav");
sfx.Play();  // one-shot playback
```

### Shader Workflow
1. Add `.hlsl` files to `StarStrike/Shaders/`
2. Configure in vcxproj: `FxCompile` with ShaderModel 6.7
3. Build outputs `CompiledShaders/%(Filename).h` with bytecode array
4. Include and use:
```cpp
#include "CompiledShaders/BasicVS.h"
// Use compiled shader bytecode in PSO setup
```

### Asset Loading
Assets resolve from `{exe_path}\Assets\` via `FileSys::GetHomeDirectory()`:
```cpp
auto data = BinaryFile::ReadFile(L"texture.bmp");
auto text = TextFile::ReadFile(L"config.txt");
```

## Testing
No automated testing infrastructure. Verify changes by running the application.

## Agent Workflow
1. Understand the scope (engine, game, or placeholder library).
2. Follow naming conventions and use modern C++20 patterns.
3. Check for DirectX 12 and Neuron engine Best practices.
4. Build with Visual Studio to verify compilation.
5. Run tests manually in the application.

## Key Files Reference
- `.github/copilot-instructions.md` - Detailed engine documentation
- `NeuronClient/GameMain.h` - Game framework interface
- `NeuronClient/GraphicsCore.h` - Graphics subsystem
- `NeuronClient/AudioCore.h` - Audio subsystem
- `StarStrike/WinMain.cpp` - Application entry point
