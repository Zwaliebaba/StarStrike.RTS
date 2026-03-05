# Copilot Instructions

> **Coding standards:** Always follow [CODE_STANDARDS.md](../CODE_STANDARDS.md) for naming conventions, memory management, error handling, file organisation, and style rules. The sections below provide **project-specific operational context** that supplements—but never overrides—those standards. When in doubt, CODE_STANDARDS.md wins.

## Project Snapshot
- Gameplay code is in `GameLogic/`, built as a static lib referenced by every app; expect Homeworld-era globals, macros, and paired `.cpp/.h` files listed inside `GameLogic.vcxproj`.
- The Win32 shell in `InterstellarOutpost/` boots `ClientEngine::Startup`, instantiates `GameApp` (a thin `GameMain` wrapper), then hands control to the legacy `StartGame()` loop defined in `InterstellarOutpost/main.cpp`.
- Engine plumbing is split between `NeuronCore/` (math, timers, file system, event bus) and `NeuronClient/` (window creation, DPI/layout, input translation, DX instrumentation); the game app links all of them plus `SinglePlayer/`.
- Campaign content in `SinglePlayer/` is auto-generated from `.kas` scripts into `.c/.h` via `SinglePlayer/Generate.cmd` and the `Tools/kas2c.exe` converter; commit contributions at the source `.kas` level.

## Build & Run
- Install native deps once with `vcpkg install --triplet x64-windows` (manifest specifies `libjpeg-turbo` for texture decoding).
- Restore NuGet feeds (`nuget restore InterstellarOutpost.slnx` or let Visual Studio do it) to pull the WinAppSDK/WinPix packages declared in each `packages.config`.
- Build via Visual Studio 2022 (v145 toolset) or `msbuild InterstellarOutpost.slnx /p:Configuration=Debug /p:Platform=x64`; `GameLogic` compiles as static lib, `InterstellarOutpost` emits the MSIX-packaged desktop app.
- Assets under `InterstellarOutpost/Assets` are marked as `DeploymentContent` in the vcxproj; run from the app output directory so `FileSys::SetHomeDirectory` (set in `WinMain.cpp`) can resolve resources.

## Coding Patterns
- all code must follow the naming, style, and memory-management rules in [CODE_STANDARDS.md](../CODE_STANDARDS.md) (PascalCase classes, `m_` members, camelCase functions, smart pointers, `#pragma once`, modern C++23 idioms).
- Every module uses `pch.h`; add new translation units to the relevant `.vcxproj` and include the precompiled header first.
- Use `DebugTrace` and `DEBUG_ASSERT` (from `NeuronCore/Debug.h`) for diagnostics and invariants, as specified in CODE_STANDARDS.md.
- Prefer `std::unique_ptr` / `std::shared_ptr` for new allocations. Use the `NEW` macro when debug-tracked raw allocation is needed, and `MALLOC`/`FREE` only in legacy code paths.
- Windows messages fan through `ClientEngine::WndProc` → `EventManager::WndProc` → legacy processors registered via `EventManager::AddEventProcessor` (`InterstellarOutpost/main.cpp` registers `WindowProc`). Return `-1` when you want the next handler to run.
- The main simulation tick is the `while(TRUE)` loop in `StartGame()` that pumps Win32 messages and then calls `utyTasksDispatch()`. Any gameplay system work must be re-entrant with that loop and rely on globals initialized by `utyGameSystemsInit()`.

## Subsystem Notes
- `GameLogic/` folders map to gameplay domains (AI, rendering, networking, ship classes, UI). Follow the existing naming (e.g., `DefenseFighter.cpp` + `.h`) and keep new helpers near their subsystem. Route platform needs through `NeuronCore` or `NeuronClient` — do not add direct Win32/DirectX calls in `GameLogic/` files (see CODE_STANDARDS.md § Platform Abstractions).
- `NeuronCore/` owns reusable utilities (`FileSys`, `Timer::Core`, `EventManager`). For file IO, prefer `Neuron::BinaryFile::ReadFile` so the home directory + `Assets` suffix is respected.
- `NeuronClient/` owns the window/device lifecycle (`ClientEngine`, `GameMain`, string localization). Hook device events via `IDeviceNotify` overrides instead of touching Win32 APIs directly.
- `SinglePlayer/` missions pair `.cpp/.h` files with `.kas` sources; regenerate via `Generate.cmd` whenever scripts change, and avoid editing the generated C directly.

## Data & Tools
- `InterstellarOutpost/Assets` contains `.btg` nebula backdrops, localized credits, `.script` files, etc.; respect the folder structure because MSIX packaging uses the same relative paths.
- Use `Tools/kas2c.exe` via the provided batch file to convert `.kas` to compiled missions. The script relies on the VS `cl` preprocessor (`cl /E`), so ensure a Developer Command Prompt environment when running it.
- Sounds and speech samples live under `External/Sound/`; loader code in `NeuronClient/sound*.cpp` expects wave data converted through the existing tooling.

## Dependency Notes
- NuGet packages pull WinAppSDK 1.8 components plus `Microsoft.Windows.CppWinRT` and `WinPixEventRuntime`—stay on the pinned versions unless the solution requires synchronized updates across all vcxproj files.
- The project enforces Unicode/Win64 builds; Win32 configs exclude several heavy AI files (see conditions inside `GameLogic.vcxproj`), so test on x64 before committing.
- When new third-party libs are needed, add them to both `vcpkg.json` and the consuming vcxproj `AdditionalIncludeDirectories`/`AdditionalDependencies` so MSBuild + manifest mode stay in sync.
