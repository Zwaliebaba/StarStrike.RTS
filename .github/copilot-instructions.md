# Copilot Instructions for StarStrike.RTS

## Project Overview

StarStrike is a DirectX 12 space game built on the Neuron engine framework. The codebase has three main projects:

- **NeuronCore** (`NeuronCore/`): Foundation utilities - math, file system, timers, debugging
- **NeuronClient** (`NeuronClient/`): DirectX 12 graphics engine, audio (XAudio2), input handling
- **StarStrike** (`StarStrike/`): The game itself - extends `GameMain` to implement game logic

## Architecture Patterns

### Namespace Hierarchy
All engine code uses nested namespaces under `Neuron`:
- `Neuron::Graphics` - DirectX 12 rendering (see [GraphicsCore.h](../NeuronClient/GraphicsCore.h))
- `Neuron::Audio` - XAudio2 sound engine
- `Neuron::Timer` - Frame timing
- `Neuron::Client` - High-level application engine
- `Neuron::Math` - DirectXMath wrappers

### Static Singleton Pattern
Core engine systems use static class methods with `inline static` member variables instead of instance singletons:
```cpp
class Core {
public:
    static void Startup();
    static ID3D12Device10* GetD3DDevice() noexcept { return m_d3dDevice.get(); }
private:
    inline static com_ptr<ID3D12Device10> m_d3dDevice;
};
```

### GameMain Base Class
Games implement the `GameMain` interface from [GameMain.h](../NeuronClient/GameMain.h):
```cpp
class StarStrike : public GameMain {
    void Startup() override;   // Initialize game state
    void Shutdown() override;  // Cleanup
    void Update(float _deltaT) override;  // Game logic per frame
    void RenderScene() override;          // 3D rendering
    void RenderCanvas() override;         // UI overlay (LDR)
};
```

## Coding Conventions

### Naming
- **Parameters**: Prefix with underscore: `_deltaT`, `_size`, `_hInstance`
- **Member variables**: `m_` prefix for instance, `sm_` prefix for static members
- **Classes/Methods**: PascalCase (`ClientEngine`, `GetCommandList`)
- **Local variables**: camelCase

### Legacy Code
StarStrike contains ported "Elite: The New Kind" code with C-style conventions (e.g., `space.h`, `trade.h`, `elite.h`). **New code should always use modern C++ patterns** regardless of the style in adjacent legacy files. Do not match legacy naming or patterns.

### Resource Management
- Use `winrt::com_ptr<T>` for COM objects (D3D12 resources)
- RAII patterns via `inline static` lifetime management
- All engine systems follow `Startup()`/`Shutdown()` lifecycle

### Modern C++
- C++20 standard (`std::format`, `std::ranges`, `[[nodiscard]]`)
- DirectXMath types for vectors/matrices (`XMVECTOR`, `XMMATRIX`)
- WinRT for Windows integration

## Build & Dependencies

### Build System
- Visual Studio solution: `StarStrike.slnx`
- Platform: **x64 only**
- Run clang-format before committing (config: `.clang-format`)

### Package Management
- **vcpkg**: `sdl1`, `sdl1-mixer` (see `vcpkg.json`)
- **NuGet**: Windows App SDK, CppWinRT, PIX profiler (see `packages.config` files)

### Asset Loading
Assets are resolved via `FileSys::GetHomeDirectory()` which points to `{exe_path}\Assets\`. Use `BinaryFile::ReadFile()` or `TextFile::ReadFile()` for loading.

### Shaders
Shaders are added manually to the project and compile to `.h` header files containing bytecode arrays. Include the generated header and pass the bytecode to PSO setup methods.

### Testing
No testing infrastructure is currently available.

## Key Integration Points

### Graphics Pipeline
1. `Graphics::Core` manages D3D12 device, swap chain, command lists
2. Use `Graphics::Core::GetCommandList()` for rendering commands
3. Resources tracked via `ResourceStateTracker` for barrier management
4. Descriptors allocated via `DescriptorAllocator::Allocate()`

### Creating Pipeline States
See [PipelineState.h](../NeuronClient/PipelineState.h):
```cpp
GraphicsPSO pso(L"MyPSO");
pso.SetRootSignature(rootSig);
pso.SetVertexShader(vsBlob, vsSize);
pso.SetPixelShader(psBlob, psSize);
pso.Finalize();
```

### Debug Utilities
```cpp
Neuron::DebugTrace("Value: {}", value);  // Debug output (release: no-op)
ASSERT(condition);                        // Fatal in all builds
DEBUG_ASSERT(condition);                  // Debug-only assertion
```
