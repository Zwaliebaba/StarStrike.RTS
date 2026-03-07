# Copilot Instructions

> **Coding standards:** Always follow [CODE_STANDARDS.md](../CODE_STANDARDS.md) for naming conventions, memory management, error handling, file organisation, and style rules. The sections below provide **project-specific operational context** that supplements—but never overrides—those standards. When in doubt, CODE_STANDARDS.md wins.

## Project Snapshot
- StarStrike.RTS is a greenfield voxel-based isometric RTS MMO written in C++23, targeting Windows 10+ x64.
- Engine plumbing is split between `lib/NeuronCore/` (math, timers, file system, event bus) and `lib/NeuronClient/` (window creation, DX12 rendering pipeline, input, game lifecycle); gameplay code will go in `lib/GameLogic/` (currently an INTERFACE placeholder).
- The Win32 shell in `StarStrike/` boots `ClientEngine::Startup`, instantiates `GameApp` (a concrete `GameMain`), then hands control to the message loop in `NeuronClient.cpp`.
- Server code will live in `lib/NeuronServer/` (INTERFACE placeholder) and `Server/` (headless executable placeholder).

## Build & Run
- Build system: CMake 4.1.2, Ninja generator, vcpkg manifest mode.
- Configure: `cmake --preset x64-debug` (requires VS Developer Command Prompt with `VCPKG_ROOT` set).
- Build: `cmake --build out/build/x64-debug`.
- vcpkg dependencies (current): `cppwinrt`, `winpixevent`. Future: `yaml-cpp`, `zstd`, `nlohmann-json`. Database: MS SQL Server via ODBC (Windows SDK built-in, no vcpkg dependency).
- Run from the app output directory so `FileSys::SetHomeDirectory` (set in `WinMain.cpp`) can resolve resources.

## Coding Patterns
- All code must follow the naming, style, and memory-management rules in [CODE_STANDARDS.md](../CODE_STANDARDS.md) (PascalCase classes, `m_` members, camelCase functions, smart pointers, `#pragma once`, modern C++23 idioms).
- Every module uses `pch.h`; add new translation units to the relevant `.vcxproj` and include the precompiled header first (MSBuild handles PCH via `<PrecompiledHeader>` settings).
- **GUI apps** (StarStrike / NeuronClient): Use `DebugTrace` (debug-only, `OutputDebugString`) and `Fatal` from `NeuronCore/Debug.h`.
- **Console apps** (Server / NeuronServer): Use `LogInfo`, `LogWarn`, `LogError`, `LogFatal` from `NeuronServer/ServerLog.h` — these write to stdout/stderr so output is visible in terminals, Docker logs, and CI. **Do not** use `DebugTrace` in server-side code.
- Prefer `std::unique_ptr` / `std::shared_ptr` for new allocations.
- Windows messages fan through `ClientEngine::WndProc` → `WndProcManager` → processors registered via `WndProcManager::AddProcessor`. Return `-1` when you want the next handler to run.

## Subsystem Notes
- `lib/GameLogic/` will contain shared gameplay logic (AI, entities, voxels) used by both client and server. Route platform needs through `NeuronCore` or `NeuronClient` — do not add direct Win32/DirectX calls in `GameLogic/` files (see CODE_STANDARDS.md § Platform Abstractions).
- `lib/NeuronCore/` owns reusable utilities (`FileSys`, `Timer`, threading stubs, socket stubs). For file IO, prefer `FileSys` so the home directory + assets path is respected.
- `lib/NeuronClient/` owns the window/device lifecycle (`ClientEngine`, `GameMain`), the full DX12 rendering pipeline (`GraphicsCore`, `DescriptorHeap`, `RootSignature`, `PipelineState`, `ResourceStateTracker`, `GpuBuffer`, `SamplerManager`, `DDSTextureLoader`), and string localization. Hook device events via `IDeviceNotify` overrides instead of touching Win32 APIs directly.
- `lib/NeuronServer/` will own database access, simulation engine, and server-side networking.

## Dependency Notes
- vcpkg dependencies are declared in `vcpkg.json`; currently `cppwinrt` and `winpixevent` (PIX profiling).
- The project enforces Unicode/Win64 builds via `UNICODE`/`_UNICODE` compile definitions in the root CMakeLists.txt.
- When new third-party libs are needed, add them to `vcpkg.json` and update the consuming `CMakeLists.txt` with `find_package` and `target_link_libraries`.
