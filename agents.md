# StarStrike.RTS – Agent Reference

## Project Overview

StarStrike.RTS is a **greenfield** voxel-based isometric RTS MMO written in **C++23**
targeting Windows. The project is in early development (Phase 1–1.5 complete). The
codebase currently has ~30 source files across three static libraries and one executable.

## Architecture

Four layers, built as CMake targets in dependency order:

```
NeuronCore (STATIC)  →  GameLogic (INTERFACE placeholder)
                     →  NeuronClient (STATIC)  →  StarStrike (WIN32 exe)
                     →  NeuronServer (INTERFACE placeholder)  →  Server (placeholder)
```

### NeuronCore (`lib/NeuronCore/`)

Foundation library: types, constants, math, timers, file I/O, debug utilities,
async helpers, event/window-proc management. STATIC library (4 .cpp files).

Key files:
| File | Purpose |
|---|---|
| `Types.h` | `EntityID`, `PlayerID`, `ChunkID`, `Vec2`/`Vec3`/`Vec3i`, `Quat`, `AABB`, enums |
| `Constants.h` | Tick rate (60 Hz), chunk/sector geometry, network limits, physics AABBs |
| `NeuronCore.cpp/.h` | Core initialization and platform bootstrap |
| `FileSys.cpp/.h` | File system utilities (home directory, asset resolution) |
| `WndProcManager.cpp/.h` | Win32 message routing chain |
| `Socket.h` | `UDPSocket` class stub (WinSock2-based, move-only) |
| `Timer.h` | `steady_clock`-based delta timer (fully implemented inline) |
| `Threading.h` | `ThreadPool` class stub (PIMPL pattern) |
| `config.h.in` | CMake-configured compile-time constants |

### GameLogic (`lib/GameLogic/`)

INTERFACE placeholder — will contain shared gameplay logic used by both client and server.

### NeuronClient (`lib/NeuronClient/`)

Engine-level STATIC library (11 .cpp files) owning the DirectX 12 rendering pipeline,
window lifecycle, and game application framework.

Key files:
| File | Purpose |
|---|---|
| `GraphicsCore.cpp/.h` | D3D12 device, swap chain, command queue/list, fence sync, debug layer |
| `DescriptorHeap.cpp/.h` | CPU + GPU descriptor allocation (CBV/SRV/UAV/Sampler/RTV/DSV) |
| `RootSignature.cpp/.h` | Builder pattern for root signatures |
| `PipelineState.cpp/.h` | GraphicsPSO wrapper (depth, blend, rasterizer, input layout) |
| `ResourceStateTracker.cpp/.h` | Batched resource barriers (up to 16), split barrier support |
| `GpuBuffer.cpp/.h` | RAII D3D12 buffer with state tracking |
| `SamplerManager.cpp/.h` | Hash-based sampler descriptor deduplication |
| `DDSTextureLoader.cpp/.h` | DDS file/memory → D3D12 resource (mipmaps, cubemaps) |
| `NeuronClient.cpp/.h` | `ClientEngine`: Win32 window creation, message loop, `GameMain` lifecycle |
| `GameMain.h` | Abstract base with lifecycle hooks (`Update`, `RenderScene`, `RenderCanvas`) |

### NeuronServer (`lib/NeuronServer/`)

INTERFACE placeholder — will contain database, simulation engine, and server-side networking.

### StarStrike (`StarStrike/`)

Win32 desktop executable (3 .cpp files). Links NeuronClient, GameLogic, NeuronCore.

| File | Purpose |
|---|---|
| `WinMain.cpp` | Entry point, home directory setup |
| `GameApp.cpp/.h` | Concrete `GameMain` — language detection, touch input, WndProc |

### Server (`Server/`)

Placeholder — will be the headless game server executable.

## Technology Stack

| Component | Details |
|---|---|
| **Language** | C++23 (MSVC 19.50, VS 2026 v18) |
| **Build system** | CMake 4.1.2, Ninja generator, vcpkg manifest mode |
| **Graphics** | DirectX 12 (d3d12.lib, dxgi.lib, dxguid.lib) |
| **Profiling** | PIX (WinPixEventRuntime via vcpkg) |
| **Networking** | WinSock2 UDP (custom protocol, planned) |
| **Persistence** | PostgreSQL 14+ (planned, via libpq) |
| **Platform** | Windows 10+ x64 only |
| **Server deployment** | Windows Server Core containers (planned) |

## Build System

```powershell
# Configure (from VS Developer Command Prompt with VCPKG_ROOT set)
cmake --preset x64-debug        # or x64-release

# Build
cmake --build out/build/x64-debug

# Verify zero errors
cmake --build out/build/x64-debug 2>&1 | Select-String -Pattern "error"
```

Available presets (see `CMakePresets.json`):

| Preset | Architecture | Config |
|---|---|---|
| `x64-debug` | 64-bit | Debug |
| `x64-release` | 64-bit | Release |

**vcpkg dependencies** (current): `cppwinrt`, `winpixevent`
**vcpkg dependencies** (planned): `libpq`, `yaml-cpp`, `zstd`, `nlohmann-json`

## Coding Conventions

See [`CODE_STANDARDS.md`](CODE_STANDARDS.md) for the full style guide. Quick reference:

- **C++23** compiled via MSVC — PascalCase classes, `m_` member prefix, camelCase functions
- Smart pointers (`std::unique_ptr` / `std::shared_ptr`) for new allocations
- Every translation unit includes `pch.h` first (per-target precompiled headers via CMake)
- `DebugTrace` and `DEBUG_ASSERT` (from `NeuronCore/Debug.h`) for diagnostics
- `#pragma once` header guards
- No direct Win32/DirectX calls in `GameLogic/` — route through `NeuronCore` or `NeuronClient`

## PCH Strategy

Each target owns a `pch.h` → `pch.cpp` pair, compiled via `target_precompile_headers(PRIVATE pch.h)`:

- **NeuronCore/pch.h:** STL headers + Windows + WinRT + math + `NeuronCore.h`
- **NeuronClient/pch.h:** Adds DX12, audio, graphics — includes `NeuronClient.h`
- **StarStrike/pch.h:** Includes `NeuronClient.h` (inherits full chain)

## Git Workflow

- Main integration branch: `master`
- Feature branches: `feature/<description>`
- Commit messages: short imperative sentences
