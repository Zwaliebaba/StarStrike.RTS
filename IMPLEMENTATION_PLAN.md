# StarStrike.RTS — 14-Week Implementation Plan

**Document Created:** March 5, 2026  
**Last Updated:** March 10, 2026  
**Target Timeline:** 10–14 weeks to MVP  
**Current Status:** Phase 1 Complete, Phase 1.5 Complete, MSBuild Migration Complete, Phase 2 Complete, Phase 2.5 Complete, Phase 3 Complete — Phase 4 implementation next  
**Primary Reference:** StarStrike.md, ARCHITECTURE_RECOMMENDATIONS.md  
**Validation:** IMPLEMENTATION_VALIDATION.md (March 5, 2026) — all recommendations incorporated

---

## Overview

This plan guides incremental implementation of a persistent voxel-based isometric RTS MMO from architecture to playable MVP. Each of 6 phases leaves the project buildable, linkable, and runnable. The plan prioritizes forward declarations to minimize PCH thrashing, respects the library architecture (NeuronCore → GameLogic → {NeuronClient, NeuronServer} → {StarStrike, Server}), and integrates WinSock2 for Windows networking from the start.

alongside vcpkg manifest mode (`vcpkg.json`) for yaml-cpp and winpixevent.

> **Validation Update (March 6, 2026):** Timeline extended from 12 to 14 weeks based on IMPLEMENTATION_VALIDATION.md review. Key additions: per-phase unit/integration test requirements, formalized network protocol with error handling, quantitative performance tracking from Phase 2, concrete risk mitigation checklists, finalized database schema with locking/indices, shader compilation MSBuild integration, and Audio/UI placeholder phase (6.5). See each phase for specific additions marked with 🆕.

> **Phase 1.5 Update (March 6, 2026):** The full DirectX 12 client engine boilerplate has been implemented ahead of schedule, completing ~90% of the planned Phase 4 DX12 work. This includes the D3D12 device manager (`GraphicsCore`), descriptor heaps, pipeline state objects, root signatures, resource state tracking, GPU buffers, DDS texture loading, sampler management, PIX profiling, window creation (`ClientEngine`), the game application framework (`GameMain` / `GameApp`), and the Win32 entry point. NeuronCore was upgraded to a STATIC library with project-level `pch.h` precompiled headers per target. The implementation uses different file names from the original plan (e.g. `GraphicsCore` instead of `DX12Device`) — the plan has been updated to reflect reality. See Phase 1.5 for details.

> **Phase 2 Update (March 8, 2026):** Server Network & Database Foundation is complete. All Phase 2 boilerplate files (scaffolded during Phase 1.5) have been promoted to full implementations: `UDPSocket` (WinSock2 non-blocking), MS SQL Server connection pool via ODBC (`Database` with configurable pool size + slow query logging),

> **Phase 2.5 Update (March 10, 2026):** Unit Test Project & Phase 1/2 Test Coverage is complete. The `Tests.NeuronCore` Microsoft Native Unit Test DLL project was added to the solution with full tests for Phase 1 (Types, Constants, ChunkID, Vec3, AABB) and Phase 2 (PacketCodec round-trip for all packet types, CRC corruption detection, magic mismatch, too-short/oversized packets, CRC32 consistency). All 82 tests pass via `vstest.console.exe`.

Phase 3 unit tests added: EntitySystem (9 tests including 100K free pool correctness), RLE codec (7 tests: empty/sparse/dense/hollow-sphere/alternating/malformed), VoxelSystem (5 tests: load/set/delta/bounds), Sector (10 tests: bounds/mapping/manager). All 82 tests pass (0 failures).

**Success Criteria:**
- Phase 1–3: Systems build and run independently (no gameplay yet)
- Phase 4: 50 concurrent players, ships moving, 60 Hz tick maintained
- Phase 5: Combat, mining, voxel destruction replicated to all clients
- Phase 6: Full persistence, graceful shutdown, observability
- Phase 6.5: Audio & UI placeholder (post-MVP polish)
- Final: MVP ships with <$50/month server hosting cost, 60 FPS+ client

---

## Requirements

### Functional Requirements
- **Network:** Server-authoritative UDP (WinSock2) at 7777, 60 Hz tick, 20 Hz client snapshots
- **Voxel World:** 4×4 sectors, 32×32×32 chunks, visible destruction for all players
- **Combat:** Ship-to-ship targeting, projectile weapons, hit detection, damage & respawn
- **Mining:** Voxel extraction, resource collection, cargo system
- **Persistence:** MS SQL Server (ODBC) with RLE compression, voxel event log, player ship state
- **Rendering:** DirectX 12, greedy-meshed voxels, pixelated post-process, bloom glow
- **Concurrency:** ≥50 simultaneous players without tick rate degradation

### Performance Targets
- **Server Tick:** < 16.67 ms @ 60 Hz (100% utilization at 50 players)
- **Network:** ~1.5 KB/s inbound, ~14 KB/s outbound per player
- **Client Frame:** 60+ FPS with 200 visible chunks on Windows x64 (mid-range GPU)
- **Memory:** Server 4 GB, Client 2 GB
- **Persistence:** Voxel events flushed every 1 sec, chunks every 30 sec

### Architecture Constraints
- **No Exceptions/RTTI** in GameLogic (game code)
- **C++23** (MSVC v145 toolset minimum)
- **MSBuild** (Visual Studio `.vcxproj` / `.slnx`), NuGet + vcpkg for package management
- **WinSock2** for all networking (Windows-only target)
- **Library Layering:** `NeuronCore` (deps: none) → `GameLogic` + `NeuronServer` + `NeuronClient` → `Server/` (exe) + `StarStrike/` (exe)
- **Unit Testing:** Microsoft Native Unit Test framework (`CppUnitTest.h` / `Microsoft.VisualStudio.TestTools.CppUnitTestFramework`); test DLL projects per library, run via VS Test Explorer or `vstest.console.exe`. All network testing uses the real UDP stack (no mock sockets).
- **Logging Convention:** GUI apps (StarStrike) use `DebugTrace` (`OutputDebugString` — debug-only). Console apps (Server, NeuronServer) use `ServerLog.h` (`LogInfo`/`LogWarn`/`LogError`/`LogFatal` — stdout/stderr, always active).
- **Directory Convention:** No `/src`, `/include`, or `/lib` subdirectories — each project is a top-level folder; `.cpp` and `.h` files live together in each project folder

---

## Architecture Changes Summary

### New Files & Directories

| File / Path | Project | Purpose | Dependencies | Status |
|---|---|---|---|---|
| **NeuronCore/Types.h** | NeuronCore | EntityID, ChunkID, Vec3, Quat typedefs | none | ✅ Done |
| **NeuronCore/Constants.h** | NeuronCore | Chunk sizes, tick rate, sector grid | none | ✅ Done |
| **NeuronCore/Socket.h** | NeuronCore | UDP socket manager, WinSock2 wrapper (header) | winsock2.lib | ✅ Done (header) |
| **NeuronCore/Socket.cpp** | NeuronCore | UDP socket implementation | winsock2.lib | ✅ Done |
| **NeuronCore/GameMath.h** | NeuronCore | DirectXMath vector/matrix wrappers | DirectXMath | ✅ Done |
| **NeuronCore/MathCommon.h** | NeuronCore | Bit operations, alignment, power-of-two | none | ✅ Done |
| **NeuronCore/Hash.h** | NeuronCore | CRC32 with SSE4.2/ARM64 SIMD | none | ✅ Done |
| **NeuronCore/Debug.h** | NeuronCore | DebugTrace, Fatal, ASSERT macros | none | ✅ Done |
| **NeuronCore/NeuronHelper.h** | NeuronCore | Enum helpers, BaseException | none | ✅ Done |
| **NeuronCore/FileSys.cpp/.h** | NeuronCore | Win32 binary/text file I/O | Windows.h | ✅ Done |
| **NeuronCore/FileSystem.h** | NeuronCore | std::filesystem-based file API | std::filesystem | ✅ Done |
| **NeuronCore/Threading.h** | NeuronCore | Thread pool (enqueue, waitAll) | std::thread | ✅ Done |
| **NeuronCore/Timer.h** | NeuronCore | High-res chrono timer | std::chrono | ✅ Done |
| **NeuronCore/TimerCore.h** | NeuronCore | QPC-based frame timer with fixed timestep | Windows.h | ✅ Done |
| **NeuronCore/ASyncLoader.h** | NeuronCore | Async loading base class | std::future | ✅ Done |
| **NeuronCore/DeviceNotify.h** | NeuronCore | Device lifecycle interface | none | ✅ Done |
| **NeuronCore/WndProcManager.cpp/.h** | NeuronCore | Win32 message routing chain | Windows.h | ✅ Done |
| **NeuronCore/PacketCodec.cpp/.h** | NeuronCore | Binary packet serialization/deserialization (CRC-32C, magic, sequence) | NeuronCore | ✅ Done |
| **NeuronCore/PacketTypes.h** | NeuronCore | Packet type structs (CmdInput, SnapState, Ping, etc.) | NeuronCore | ✅ Done |
| **NeuronClient/GraphicsCore.cpp/.h** | NeuronClient | D3D12 device, swap chain, command queue, Present | d3d12, dxgi | ✅ Done |
| **NeuronClient/GraphicsCommon.cpp/.h** | NeuronClient | Global rasterizer/blend/depth states | d3d12 | ✅ Done |
| **NeuronClient/DescriptorHeap.cpp/.h** | NeuronClient | D3D12 descriptor heap allocation | d3d12 | ✅ Done |
| **NeuronClient/RootSignature.cpp/.h** | NeuronClient | Root signature builder | d3d12 | ✅ Done |
| **NeuronClient/PipelineState.cpp/.h** | NeuronClient | PSO creation (GraphicsPSO) | d3d12 | ✅ Done |
| **NeuronClient/ResourceStateTracker.cpp/.h** | NeuronClient | Batched resource barrier management | d3d12 | ✅ Done |
| **NeuronClient/GpuBuffer.cpp/.h** | NeuronClient | Vertex/index/constant buffer management | d3d12 | ✅ Done |
| **NeuronClient/GpuResource.h** | NeuronClient | Base GPU resource with state tracking | d3d12 | ✅ Done |
| **NeuronClient/SamplerManager.cpp/.h** | NeuronClient | Sampler descriptor caching | d3d12 | ✅ Done |
| **NeuronClient/DDSTextureLoader.cpp/.h** | NeuronClient | DDS texture format loader | d3d12 | ✅ Done |
| **NeuronClient/NeuronClient.cpp/.h** | NeuronClient | ClientEngine: window creation, Run loop, WndProc | Win32, WinRT | ✅ Done |
| **NeuronClient/GameMain.h** | NeuronClient | Abstract game app lifecycle (Update/Render) | WinRT | ✅ Done |
| **NeuronClient/DirectXHelper.h** | NeuronClient | D3D12 helper macros & utilities | d3d12 | ✅ Done |
| **NeuronClient/d3dx12.h** | NeuronClient | D3D12 helper structures (CD3DX12_*) | d3d12 | ✅ Done |
| **NeuronClient/VertexTypes.h** | NeuronClient | Vertex layout descriptors | d3d12 | ✅ Done |
| **NeuronClient/Color.h** | NeuronClient | 150+ named XMVECTORF32 color constants | DirectXMath | ✅ Done |
| **NeuronClient/PixProfiler.h** | NeuronClient | PIX profiling macros (debug only) | WinPixEventRuntime | ✅ Done |
| **NeuronClient/Audio.h** | NeuronClient | Audio subsystem (stub) | — | ⬜ Phase 6.5 |
| **NeuronClient/Strings.h** | NeuronClient | String localization (stub) | — | ⬜ Phase 6.5 |
| **StarStrike/GameApp.cpp/.h** | StarStrike | Game application (touch, camera, render) | NeuronClient | ✅ Done (skeleton) |
| **StarStrike/WinMain.cpp** | StarStrike | Win32 entry point + ClientEngine boot | NeuronClient | ✅ Done |
| **NeuronServer/SimulationEngine.cpp/.h** | NeuronServer | 60 Hz tick loop, 6 phases (stubs) | NeuronCore, GameLogic | ✅ Done |
| **NeuronServer/Database.cpp/.h** | NeuronServer | MS SQL Server connection pool (ODBC, configurable size, slow query logging) | odbc32 (Windows SDK) | ✅ Done |
| **NeuronServer/SocketManager.cpp/.h** | NeuronServer | Server network transport (recv/validate/dedup/send, rate-limited CRC logging) | NeuronCore | ✅ Done |
| **NeuronServer/TickProfiler.cpp/.h** | NeuronServer | Tick-time measurement/histogram (rolling 600 samples, p50/p95/p99) | NeuronCore | ✅ Done |
| **Server/main.cpp** | Server | Server entry point + main loop (SetConsoleCtrlHandler) | NeuronServer | ✅ Done |
| **Server/Config.cpp/.h** | Server | YAML config parsing (yaml-cpp) | yaml-cpp | ✅ Done |
| **config/server.yaml** | Server | Default server configuration | — | ✅ Done |
| **NeuronServer/ServerLog.h** | NeuronServer | Console-friendly stdout/stderr logging (LogInfo, LogWarn, LogError, LogFatal) | std::format | ✅ Done |
| **Tests.NeuronCore/Tests.NeuronCore.vcxproj** | Tests.NeuronCore | Microsoft Native Unit Test DLL — Phase 1, 2 & 3 tests | NeuronCore, NeuronServer, GameLogic | ✅ Done |
| **Tests.NeuronCore/pch.h/.cpp** | Tests.NeuronCore | PCH for test project (CppUnitTest.h + NeuronCore headers) | — | ✅ Done |
| **Tests.NeuronCore/TypesTests.cpp** | Tests.NeuronCore | Phase 1: Vec3/AABB/ChunkID/constants static + runtime asserts | NeuronCore | ✅ Done |
| **Tests.NeuronCore/PacketCodecTests.cpp** | Tests.NeuronCore | Phase 2: encode→decode round-trip, CRC corruption, magic mismatch, oversized | NeuronCore | ✅ Done |
| **Tests.NeuronCore/EntitySystemTests.cpp** | Tests.NeuronCore | Phase 3: spawn/destroy/free pool/tick update (9 tests incl. 100K stress) | GameLogic | ✅ Done |
| **Tests.NeuronCore/VoxelSystemTests.cpp** | Tests.NeuronCore | Phase 3: RLE codec (7 tests) + VoxelSystem CRUD (5 tests) | GameLogic | ✅ Done |
| **Tests.NeuronCore/SectorTests.cpp** | Tests.NeuronCore | Phase 3: Sector bounds/mapping + SectorManager grid (10 tests) | GameLogic | ✅ Done |
| **GameLogic/GameLogic.cpp/.h** | GameLogic | Shared gameplay logic (placeholder) | NeuronCore | ✅ Done (placeholder) |
| **GameLogic/EntitySystem.cpp/.h** | GameLogic | ECS-lite, array-of-structs with free pool ID reuse | NeuronCore | ✅ Done |
| **GameLogic/VoxelSystem.cpp/.h** | GameLogic | Chunk storage, RLE serialization, delta tracking | NeuronCore | ✅ Done |
| **GameLogic/Sector.cpp/.h** | GameLogic | Sector bounds + SectorManager 4×4 grid | NeuronCore | ✅ Done |
| **GameLogic/WorldManager.cpp/.h** | GameLogic | Top-level orchestrator (EntitySystem + VoxelSystem + SectorManager) | GameLogic | ✅ Done |
| **NeuronServer/ChunkStore.cpp/.h** | NeuronServer | Chunk persistence layer (load/save/flush, voxel events, schema DDL) | NeuronServer, GameLogic | ✅ Done |
| **config/schema.sql** | — | MS SQL Server DDL (voxel_chunks, voxel_events, players, ships + indices) | — | ✅ Done |
| **NeuronClient/VoxelRenderer.cpp/.h** | NeuronClient | Greedy meshing, VB/IB upload | NeuronCore | ⬜ Phase 5 |
| **NeuronClient/SnapshotDecoder.cpp/.h** | NeuronClient | Deserialize snapshots | NeuronCore | ⬜ Phase 4 |
| **StarStrike/shaders/*.hlsl** | StarStrike | Voxel, entity, post-process shaders | DirectXMath | ⬜ Phase 5 |

### Modified Files

| File | Changes | Status |
|---|---|---|
| StarStrike.slnx | XML solution file; 7 projects (NeuronCore, NeuronClient, GameLogic, NeuronServer, Server, StarStrike, Tests.NeuronCore) | ✅ Done |
| vcpkg.json | cppwinrt, winpixevent, yaml-cpp; builtin-baseline | ✅ Done |
| NeuronCore/NeuronCore.vcxproj | StaticLibrary (6 .cpp), PCH via `pch.h`/`pch.cpp` (Create for Debug+Release), CppWinRT NuGet, `stdcpplatest` (Debug) / `stdcpp20` (Release) | ✅ Done |
| NeuronClient/NeuronClient.vcxproj | StaticLibrary (11 .cpp), PCH, references NeuronCore, CppWinRT NuGet, include dirs `$(SolutionDir)NeuronCore` | ✅ Done |
| StarStrike/StarStrike.vcxproj | Application (Windows App SDK / WinUI / MSIX), references NeuronClient + NeuronCore, CppWinRT + WinAppSDK NuGet | ✅ Done |
| GameLogic/GameLogic.vcxproj | StaticLibrary (7 .cpp: pch, GameLogic, EntitySystem, VoxelSystem, Sector, WorldManager), PCH (Create for Debug+Release), CppWinRT NuGet, include dirs `$(SolutionDir)NeuronCore` (Debug+Release) | ✅ Done (updated Phase 3) |
| NeuronServer/NeuronServer.vcxproj | StaticLibrary (6 .cpp: +ChunkStore), PCH (Create for Debug+Release), references NeuronCore, CppWinRT NuGet, include dirs `$(SolutionDir)NeuronCore;$(SolutionDir)GameLogic` (Debug+Release) | ✅ Done (updated Phase 3) |
| Server/Server.vcxproj | Console Application (3 .cpp), PCH (Create for Debug+Release), references GameLogic + NeuronCore + NeuronServer, include dirs `$(SolutionDir)NeuronCore;$(SolutionDir)NeuronServer;$(SolutionDir)GameLogic` (Debug+Release), Subsystem: Console | ✅ Done (updated Phase 3) |
| Tests.NeuronCore/Tests.NeuronCore.vcxproj | DynamicLibrary (6 test .cpp), PCH, references NeuronCore + NeuronServer + GameLogic, include dirs `$(SolutionDir)NeuronCore;$(SolutionDir)NeuronServer;$(SolutionDir)GameLogic`, MS CppUnitTest lib | ✅ Done (Phase 2.5 + Phase 3) |

---

## Implementation Steps

### Phase 1: Core Foundation & Types (Weeks 1–1.5, ~12 files) ✅ COMPLETE

**Status:** Completed March 5, 2026. All 12 files created; solution configures and builds with zero errors on MSVC 19.50. Build verification passed all static assertions. (Note: the original `VerifyPhase1.exe` target was replaced by the real `StarStrike.exe` build during Phase 1.5.)

**Goal:** Establish NeuronCore library with types and constants; verify build system + vcpkg + PCH.

#### Step 1.1: Initialize Build System & vcpkg ✅
- **File:** `StarStrike.slnx`, `vcpkg.json`, per-project `.vcxproj` files, per-project `packages.config`
- **Action:** Set up MSBuild solution with `.slnx` XML format; configure vcpkg manifest mode with dependencies (yaml-cpp, winpixevent). NuGet `packages.config` per project for CppWinRT (all projects) and Windows App SDK (StarStrike only). Database access uses ODBC (ships with Windows SDK — no vcpkg dependency).
- **Actual:** Visual Studio 2022+ with v145 toolset. NuGet restores CppWinRT 2.0.250303.1 and Windows App SDK 1.8. vcpkg manifest mode provides yaml-cpp, winpixevent.
- **Why:** Enables deterministic, reproducible builds with Visual Studio IDE and CLI
- **Dependencies:** None (new files)
- **Build Impact:** Low (configuration only, no compilation yet)
- **Risk:** Low (standard MSBuild patterns)

#### Step 1.2: Create NeuronCore Library (Header-Only Base Types) ✅
- **File:** `NeuronCore/Types.h`, `NeuronCore/Constants.h`
- **Action:** Defined `EntityID` (u32), `PlayerID` (u16), `ChunkID` (u64) with `INVALID_*` sentinels and `makeChunkID()` encoder. Math primitives: `Vec2`, `Vec3` (with operator+/-/*/+=), `Vec3i` (with `==`), `Quat`, `AABB` (with `contains()`/`overlaps()`). Enums: `EntityType`, `VoxelType` (empty + 8 terrain/special types), `ActionType`, `ShipType`, `PacketType` (client→server + server→client). Constants: tick rate (60 Hz), chunk/sector geometry, network limits (magic, MTU, rate limits), persistence intervals, physics AABB sizes, rendering budgets.
- **Why:** Establishes shared vocabulary for both server and client; no dependencies
- **Dependencies:** C++ standard library only
- **Build Impact:** Zero (header only)
- **Risk:** Low

#### Step 1.3: Set Up NeuronCore Project ✅
- **File:** `NeuronCore/NeuronCore.vcxproj`, `NeuronCore/pch.h`
- **Action:** Created `NeuronCore` as StaticLibrary project with include directories and PCH propagation (17 STL headers). Platform-specific link: ws2_32 on Windows. Added `NeuronCore/pch.h` precompiled header consuming all STL + NeuronCore headers.
- **Why:** Centralizes PCH generation; 60–70% rebuild speedup on subsequent changes
- **Dependencies:** None (headers only)
- **Build Impact:** Medium (triggers once at project setup)
- **Risk:** Low (standard MSBuild practice)

#### Step 1.4: Stub Out Neuron Platform Library Structure ✅
- **File:** `NeuronCore/Socket.h`, `FileSystem.h`, `Threading.h`, `Timer.h`
- **Action:** Created full API-designed header stubs (declarations only, no .cpp):
  - **Socket.h:** `UDPSocket` class — `bind()`, `sendTo()`, `recvFrom()` (returns `optional<Datagram>`), WinSock2-based (`SOCKET` handle), move-only.
  - **FileSystem.h:** `FileSystem` class — `setHomeDirectory()`, `readBinaryFile()`, `readTextFile()`, `writeBinaryFile()`, `exists()` using `std::filesystem`.
  - **Threading.h:** `ThreadPool` class — `enqueue()`, `waitAll()`, `shutdown()`, PIMPL pattern.
  - **Timer.h:** `Timer` class — `tick()` (returns delta seconds), `elapsedSec()`, `elapsedUs()`, fully implemented inline using `steady_clock`.
- **Why:** Prevents circular includes; sketches interface early
- **Dependencies:** NeuronCore headers
- **Build Impact:** Very low (headers only)
- **Risk:** Low

#### Step 1.5: Verify Build ✅
- **File:** `tests/VerifyPhase1.cpp` *(removed in Phase 1.5 — replaced by StarStrike.exe build verification)*
- **Action:** Created verification executable with 15+ `static_assert` checks covering: constant values (tick rate, chunk size, sector count, player count), ChunkID encoding, enum sizes, Vec3 arithmetic, AABB containment/overlap, and a runtime Timer smoke test. Built and ran successfully.
- **Build Result:** MSVC 19.50.35725.0, 3/3 targets compiled with zero errors, zero warnings. Verification passed. *(VerifyPhase1.exe was subsequently replaced by the StarStrike.exe target in Phase 1.5.)*
- **Why:** Catch build configuration issues early before many files depend on it
- **Dependencies:** Steps 1.1–1.4
- **Build Impact:** Zero (verification only)
- **Risk:** Very low

**Deliverables:**
- ✅ MSBuild solution configured with vcpkg — `StarStrike.slnx`, `vcpkg.json`, per-project `.vcxproj` + `packages.config`
- ✅ NeuronCore (types, constants) builds — `NeuronCore/Types.h`, `Constants.h`
- ✅ NeuronCore platform stubs in place — `NeuronCore/Socket.h`, `FileSystem.h`, `Threading.h`, `Timer.h`
- ✅ PCH strategy implemented — `NeuronCore/pch.h` with 17 STL headers + NeuronCore, propagated via project references
- ✅ Verification test — `tests/VerifyPhase1.cpp` passed (15+ static_asserts + runtime); target later removed in Phase 1.5 when real libraries replaced placeholders

**Success Criteria:** ✅ MET
```bash
msbuild StarStrike.slnx /p:Configuration=Debug /p:Platform=x64   # All targets, 0 errors
# Or open StarStrike.slnx in Visual Studio and Build Solution (Ctrl+Shift+B)
```

---

### Phase 1.5: Client Engine Boilerplate & DX12 Foundation (Week 1.5–2) ✅ COMPLETE

**Status:** Completed March 6, 2026. Full DirectX 12 rendering pipeline, window lifecycle, game application framework, and build system conversion from placeholders to real static libraries with per-target precompiled headers. This phase front-loads ~90% of the planned Phase 4 DX12 work.

**Goal:** Establish the complete client engine infrastructure — DX12 device, GPU resource management, window creation, game application lifecycle, and build system with proper PCH support — so that subsequent phases can focus on gameplay systems rather than engine plumbing.

#### Step 1.5.1: NeuronCore Foundation Implementation ✅
- **Files:** `NeuronCore.cpp/.h`, `GameMath.h`, `MathCommon.h`, `Hash.h`, `Debug.h`, `NeuronHelper.h`, `FileSys.cpp/.h`, `TimerCore.h`, `ASyncLoader.h`, `DeviceNotify.h`, `WndProcManager.cpp/.h`
- **Action:** Implemented the full NeuronCore foundation layer:
  - **NeuronCore.cpp/.h:** Umbrella header pulling in STL + Windows + WinRT + DirectXMath; `CoreEngine::Startup()` initializes WinRT apartment and verifies CPU support; `CoreEngine::Shutdown()` uninitializes WinRT.
  - **GameMath.h:** DirectXMath wrapper functions — `Set()`, `Normalize()`, `Cross()`, `Dot()`, rotation helpers, length/distance calculations.
  - **MathCommon.h:** Low-level math utilities — alignment (`AlignUpWithMask`), bit scanning (`GetHighestBit`), power-of-two checks, log2 computation.
  - **Hash.h:** CRC32 hashing with SSE4.2 hardware intrinsics (fallback to ARM64 CRC).
  - **Debug.h:** `DebugTrace` (formatted debug output via `OutputDebugString`), `Fatal` (assertion with message box), `ASSERT`/`DEBUG_ASSERT` macros, debug-tracked `NEW` macro.
  - **NeuronHelper.h:** `ENUM_HELPER` macro for scoped enum iteration; `BaseException` class with a `what()` accessor.
  - **FileSys.cpp/.h:** Win32-native binary/text file I/O (`BinaryFile::ReadFile`, `TextFile::ReadFile`) with home directory management.
  - **TimerCore.h:** `Timer::Core` using `QueryPerformanceCounter` — fixed timestep support, delta time, total time, frame counting, elapsed time reset.
  - **ASyncLoader.h:** Base class for asynchronous resource loading — `StartLoading()`, `FinishLoading()`, `WaitForLoad()` with `std::future` integration and valid/loading state flags.
  - **DeviceNotify.h:** `IDeviceNotify` interface defining `OnDeviceLost()` / `OnDeviceRestored()` callbacks for the graphics subsystem.
  - **WndProcManager.cpp/.h:** `WndProcManager` — fan-out pattern for Win32 window messages; `AddWndProc()` / `RemoveWndProc()` register handler functions; first handler returning non-(-1) wins.
- **Why:** These utilities are required by every subsequent layer (NeuronClient, GameLogic, StarStrike)
- **Dependencies:** Windows SDK, WinRT, DirectXMath
- **Build Impact:** Low (NeuronCore compiles as a 6-file static library)
- **Risk:** Low

#### Step 1.5.2: DX12 Rendering Pipeline ✅
- **Files:** `GraphicsCore.cpp/.h`, `GraphicsCommon.cpp/.h`, `DescriptorHeap.cpp/.h`, `RootSignature.cpp/.h`, `PipelineState.cpp/.h`, `ResourceStateTracker.cpp/.h`, `GpuBuffer.cpp/.h`, `GpuResource.h`, `SamplerManager.cpp/.h`, `DDSTextureLoader.cpp/.h`, `DirectXHelper.h`, `d3dx12.h`, `VertexTypes.h`, `Color.h`, `PixProfiler.h`
- **Action:** Implemented the full D3D12 rendering infrastructure:
  - **GraphicsCore (replaces planned `DX12Device`):** Complete D3D12 device manager — adapter selection, device creation (Feature Level 12.0+), command queue/allocators/lists, double-buffered swap chain (DXGI 1.6), back buffer management, render target views, depth stencil buffer, fence synchronization, `Prepare()` / `Present()` frame lifecycle with resource barriers, device lost handling, HDR/tearing support, debug layer validation.
  - **DescriptorHeap:** CPU and GPU descriptor heap management with `DescriptorHandle` (CPU + GPU handles), `DescriptorAllocator` for CBV/SRV/UAV/sampler/RTV/DSV heaps, and shader-visible heap binding.
  - **RootSignature:** Builder pattern for D3D12 root signatures — `RootParameter` with `InitAsConstants()`, `InitAsConstantBuffer()`, `InitAsBufferSRV()`/`UAV()`, static sampler initialization, `Finalize()`.
  - **PipelineState:** `GraphicsPSO` wrapping `D3D12_GRAPHICS_PIPELINE_STATE_DESC` — `SetDepthStencilState()`, `SetRenderTargetFormats()`, depth/blend/rasterizer state helpers, input layout, topology configuration.
  - **ResourceStateTracker:** Batches D3D12 resource barriers (up to 16), with `TransitionResource()`, `BeginResourceTransition()` (split barriers), `InsertUAVBarrier()`, `FlushResourceBarriers()`.
  - **GpuBuffer / GpuResource:** RAII wrappers for `ID3D12Resource` — state tracking (`m_usageState`, `m_transitioningState`), version counter, vertex/index/constant buffer creation, SRV/UAV descriptor creation.
  - **SamplerManager:** Hash-based sampler descriptor deduplication — `SamplerDesc` extends `D3D12_SAMPLER_DESC` with `CreateDescriptor()` caching.
  - **DDSTextureLoader:** Loads DDS textures from file/memory into D3D12 resources — handles FOURCC formats, mipmaps, cubemaps, alpha modes, subresource initialization.
  - **DirectXHelper.h:** `IID_GRAPHICS_PPV_ARGS` macro, D3D12 handle null/unknown constants, MAKEFOURCC, SetName debug helpers.
  - **d3dx12.h:** Microsoft D3D12 helper library (CD3DX12_* convenience types for heap properties, resource descriptions, barrier descriptions, etc.).
  - **VertexTypes.h:** `VertexPosition` struct with `D3D12_INPUT_LAYOUT_DESC` metadata for pipeline state configuration.
  - **Color.h:** 150+ named web colors as `XMVECTORF32` constants (ALICE_BLUE through YELLOW_GREEN).
  - **PixProfiler.h:** PIX profiling macros (`StartProfile`, `EndProfile`, `ScopedProfile`) — enabled in debug builds via WinPixEventRuntime; no-op in release.
- **Why:** Complete GPU pipeline needed before any rendering work; enables Phase 5 to focus on game-specific rendering (voxel mesh, shaders) rather than plumbing
- **Dependencies:** d3d12.lib, dxgi.lib, dxguid.lib, WinPixEventRuntime (vcpkg)
- **Build Impact:** Medium (11-file static library with heavy DX12 headers; PCH mitigates rebuild cost)
- **Risk:** Low (standard D3D12 patterns; all code compiles with zero errors)

#### Step 1.5.3: Client Engine & Window Lifecycle ✅
- **Files:** `NeuronClient.cpp/.h`, `GameMain.h`
- **Action:** Implemented the client engine shell:
  - **ClientEngine (`NeuronClient.cpp/.h`):** `Startup()` creates Win32 window (fullscreen or bordered), initializes `GraphicsCore`, registers `WndProc` for Win32 message routing. `StartGame()` accepts a `GameMain` subclass. `Run()` implements the main loop: `TranslateMessage()` / `DispatchMessage()` for OS events, then `Timer::Core::Update()` → `GameMain::Update()` → `GraphicsCore::Prepare()` → `RenderScene()` → `ExecuteCommandList()` → `RenderCanvas()` → `Present()`. `Shutdown()` tears down in reverse order. Device lost/restored events fan through `IDeviceNotify`.
  - **GameMain.h:** Abstract base class using `winrt::implements<GameMain, IInspectable>` — defines `Startup()`, `Shutdown()`, `Update(float deltaT)`, `RenderScene()`, `RenderCanvas()`, plus hooks for window events (`OnWindowMoved`, `OnWindowSizeChanged`, `OnDisplayChange`, `OnSuspending`, `OnResuming`, `OnActivated`, `OnDeactivated`) and touch input (`AddTouch`, `UpdateTouch`, `RemoveTouch`).
- **Why:** Establishes the application lifecycle framework that all game code plugs into
- **Dependencies:** NeuronCore, GraphicsCore, WndProcManager
- **Build Impact:** Low
- **Risk:** Low

#### Step 1.5.4: Game Application & Entry Point ✅
- **Files:** `StarStrike/GameApp.cpp/.h`, `StarStrike/WinMain.cpp`
- **Action:** Implemented the game-specific application layer:
  - **GameApp:** Extends `GameMain`. Constructor detects user language (WinRT `GlobalizationPreferences`, with try/catch fallback for unpackaged desktop builds). `Startup()` registers touch window and enables mouse-in-pointer. Lifecycle methods delegate to `GameMain` base + renderer. Touch input handlers track a single active touch for camera panning. Static `WndProc` handles right-button capture for camera orbiting.
  - **WinMain.cpp:** `wWinMain()` entry point — CRT debug leak detection (`_CrtSetDbgFlag`), home directory from executable path, `ClientEngine::Startup()` → `winrt::make_self<GameApp>()` → `ClientEngine::StartGame()` → `ClientEngine::Run()` → `ClientEngine::Shutdown()`.
- **Why:** Concrete application that can launch and render; all subsequent game features plug in here
- **Dependencies:** NeuronClient, NeuronCore
- **Build Impact:** Low (3-file WIN32 executable)
- **Risk:** Low

#### Step 1.5.5: Build System — MSBuild Migration ✅
- **Files:** `StarStrike.slnx`, `NeuronCore/NeuronCore.vcxproj`, `NeuronClient/NeuronClient.vcxproj`, `StarStrike/StarStrike.vcxproj`, `vcpkg.json`, per-project `packages.config`
- **Action:** Migrated the build system from CMake/Ninja to MSBuild (`.vcxproj` + `.slnx`):
  - **Solution:** `StarStrike.slnx` (new XML format) with 6 projects. Solution files folder includes `.gitattributes`, `.gitignore`, docs, and `vcpkg.json`.
  - **NeuronCore:** StaticLibrary (6 .cpp files: `pch.cpp`, `NeuronCore.cpp`, `FileSys.cpp`, `WndProcManager.cpp`, `PacketCodec.cpp`, `Socket.cpp`). PCH via `pch.h`/`pch.cpp` (`<PrecompiledHeader>Create</PrecompiledHeader>` on `pch.cpp`). CppWinRT via NuGet. `stdcpplatest` (Debug), `stdcpp20` (Release).
  - **NeuronClient:** StaticLibrary (11 .cpp files). PCH. References NeuronCore via `<ProjectReference>`. CppWinRT via NuGet. Include dirs: `$(SolutionDir)NeuronCore`.
  - **StarStrike:** Application (Windows App SDK / WinUI, MSIX packaging). 3 .cpp files. PCH. References NeuronClient + NeuronCore. NuGet: CppWinRT, Windows App SDK 1.8, WIL, WebView2, Windows SDK Build Tools.
  - **GameLogic:** StaticLibrary (2 .cpp files: `pch.cpp`, `GameLogic.cpp`). PCH. CppWinRT via NuGet. Include dirs: `$(SolutionDir)NeuronCore`.
  - **NeuronServer:** StaticLibrary (5 .cpp files: `pch.cpp`, `Database.cpp`, `SimulationEngine.cpp`, `SocketManager.cpp`, `TickProfiler.cpp`). PCH. References NeuronCore. CppWinRT via NuGet. Include dirs: `$(SolutionDir)NeuronCore`.
  - **Server:** Console Application (3 .cpp files: `pch.cpp`, `main.cpp`, `Config.cpp`). PCH. References GameLogic + NeuronCore + NeuronServer. Include dirs: `$(SolutionDir)NeuronCore;$(SolutionDir)NeuronServer`. Subsystem: Console.
  - **vcpkg.json:** Dependencies: `cppwinrt`, `winpixevent`, `yaml-cpp` with `builtin-baseline`.
  - **Package management:** Dual approach — NuGet `packages.config` per project for CppWinRT and Windows App SDK; vcpkg manifest mode for yaml-cpp, winpixevent. Database uses ODBC (Windows SDK built-in).
  - **PCH strategy:** Each target has its own `pch.h` → `pch.cpp` pair. `pch.cpp` is marked with `<PrecompiledHeader>Create</PrecompiledHeader>`; all other `.cpp` files use `<PrecompiledHeader>Use</PrecompiledHeader>`. All projects use v145 toolset with Unicode character set.
- **Why:** Enables native Visual Studio IDE experience with solution-level build, NuGet package management, and MSIX packaging for client; PCH reduces rebuild times by 60–70%
- **Build Impact:** High (one-time restructure; all subsequent builds benefit from PCH and IDE integration)
- **Risk:** Low (verified: `msbuild StarStrike.slnx /p:Configuration=Debug /p:Platform=x64` succeeds for all projects)

**Deliverables:**
- ✅ NeuronCore static library with full foundation (math, timers, file I/O, debug, async, events)
- ✅ NeuronClient static library with complete DX12 pipeline (device, heaps, PSO, root sigs, buffers, textures, resource tracking, samplers)
- ✅ ClientEngine with Win32 window creation, message loop, and game lifecycle
- ✅ GameMain abstract base class with full lifecycle hooks
- ✅ GameApp concrete implementation with touch input and language detection
- ✅ WinMain entry point boots ClientEngine → GameApp → Run loop
- ✅ Per-target PCH (`pch.h`) via MSBuild `<PrecompiledHeader>` settings
- ✅ NuGet integration (CppWinRT, Windows App SDK) + vcpkg (winpixevent, yaml-cpp)
- ✅ UNICODE/WIN64 builds enforced

**Success Criteria:** ✅ MET
```bash
msbuild StarStrike.slnx /p:Configuration=Debug /p:Platform=x64  # All projects compile with PCH
# Or: Open StarStrike.slnx in Visual Studio → Build Solution (Ctrl+Shift+B)
# NeuronCore.lib + NeuronClient.lib compile
# StarStrike.exe links and launches a Win32 window
# DX12 debug layer active, swap chain presents correctly
```

**Impact on Later Phases:**
- **Phase 2** (Server): Unaffected — server-side work is independent
- **Phase 3** (Entities/Voxels): Unaffected — GameLogic is independent
- **Phase 4** (Client Foundation): **~90% complete** —  DX12 device (Step 4.1), window/lifecycle (Step 4.6) done. Remaining: client-side networking (Step 4.2), entity cache (Step 4.3), input translation (Step 4.4), camera system (Step 4.5)
- **Phase 5** (Rendering): Ready — all GPU infrastructure (buffers, PSO, root sigs, descriptors) available; Phase 5 can focus on voxel mesh generation, shaders, and game-specific rendering

---

### Phase 2: Server Network & Database Foundation (Weeks 2–3.5, ~18 files) ✅ COMPLETE

**Status:** Completed March 8, 2026. All server subsystems implemented: WinSock2 UDP socket, MS SQL Server connection pool via ODBC, packet codec with CRC-32C/magic/sequence validation, 60 Hz tick loop, socket manager with per-sender sequence dedup and rate-limited CRC logging, tick-time profiler with rolling histogram, YAML config, and Windows console shutdown handler. Mock UDPSocket created for offline testing. Release build configs fixed across all projects. NeuronCore.lib, NeuronServer.lib, GameLogic.lib, and Server.exe compile with zero errors.

**Goal:** UDP socket listening, MS SQL Server connection pool (ODBC), packet codec with error handling. Server runs (empty tick loop) with tick-time measurement.

> 🆕 **Validation additions:** Formalized packet format with magic/CRC/sequence fields, packet loss/reorder handling, per-phase unit tests, tick-time histogram from day 1.

#### Step 2.1: Implement Neuron::UDPSocket (WinSock2)
- **File:** NeuronCore/Socket.cpp/h
- **Action:** Implement WinSock2 UDPSocket wrapper:
  ```cpp
  // NeuronCore/Socket.h
  namespace Neuron {
    class UDPSocket {
      bool Bind(const std::string& addr, uint16_t port);
      Packet RecvFrom();  // Returns packet + sender IP:port
      bool SendTo(const Packet& p, const std::string& addr, uint16_t port);
      SOCKET m_sock;  // WinSock2 handle
    };
  }
  ```
  - Include `<winsock2.h>`, `<ws2def.h>` (added to PCH)
  - Non-blocking I/O via `ioctlsocket(m_sock, FIONBIO, ...)` or overlapped I/O
- **Why:** Centralizes socket code; enables client & server to use same interface
- **Dependencies:** NeuronCore (for types); ws2_32.lib
- **Build Impact:** Low (new translation unit)
- **Risk:** Low

#### Step 2.2: MS SQL Server Database Connection Pool (ODBC)
- **File:** NeuronServer/Database.cpp/h
- **Action:** Implement connection pool wrapper around ODBC. Methods: Connect(), Execute(), Query(), Disconnect().
  ```cpp
  // NeuronServer/Database.h
  class Database {
    bool Connect(const std::string& connstring, int pool_size);
    std::vector<std::vector<std::string>> Query(const std::string& sql);
    void Execute(const std::string& sql);
    void BeginTransaction(); void Commit();
  };
  ```
  - Use RAII: connections released on ~SqlConnection()
  - Pool: pre-allocate N connections via ODBC environment handle, reuse via mutex
  - 🆕 **Connection pool config (per validation):**
    - Pool size = 4 × num_worker_threads (configurable via YAML, not hardcoded)
    - Idle timeout = 30 sec (close idle connections to free DB slots)
    - Max query time = 1 sec with fallback (return stale data if DB slow)
    - Log slow queries (> 100 ms) via ServerLog
- **Why:** Essential for persistence; connection pooling avoids latency spikes
- **Dependencies:** ODBC (Windows SDK built-in: `sql.h`, `sqlext.h`, `odbc32.lib`)
- **Build Impact:** Low (new translation unit, links odbc32.lib — ships with Windows)
- **Risk:** Medium (database errors can cascade; add error logging)

#### Step 2.3: Packet Codec (Serialization/Deserialization)
- **File:** NeuronCore/PacketCodec.cpp/h
- **Action:** Binary serialization utilities with formalized packet framing. Define packet types (CMD_INPUT, SNAP_STATE, etc.):
  ```cpp
  // NeuronCore/PacketTypes.h
  struct CmdInput {
    uint32_t cmd_type = CMD_INPUT;
    uint32_t player_id;
    uint8_t action;  // 1=move, 2=attack, 3=mine
    float target_x, target_y, target_z;
    uint16_t target_entity_id;
  };
  
  // NeuronCore/PacketCodec.h
  bool Serialize(const CmdInput& cmd, std::vector<uint8_t>& bytes);
  bool Deserialize(const std::vector<uint8_t>& bytes, CmdInput& cmd);
  ```
  - Little-endian byte order
  - 🆕 **Formalized packet structure (per validation):**
    ```
    [Magic: u32 = 0xDEADBEEF]     // Reject non-StarStrike traffic
    [Type: u8] [Flags: u8] [Reserved: u16]
    [Sequence: u32]                // For dedup / reordering
    [Payload Size: u16]
    [CRC32: u32]                   // Over payload bytes
    [Payload: variable]
    ```
  - 🆕 **Error handling rules:**
    - Magic mismatch → drop silently (spam prevention)
    - CRC mismatch → drop, log once per IP:port per minute
    - Out-of-order → buffer in reorder window (up to 1 sec / 60 ticks); timeout old packets
    - Duplicate → compare sequence number; drop if seen in last 60 ticks
    - Oversized packet (> MTU) → drop, log warning
  - 🆕 **Unit tests (required before phase exit):**
    ```cpp
    TEST(PacketCodec, RoundTrip) { /* serialize → deserialize all packet types */ }
    TEST(PacketCodec, CRCDetectsCorruption) { /* corrupt 1 byte, verify CRC fails */ }
    TEST(PacketCodec, MagicMismatch) { /* wrong magic → reject */ }
    TEST(PacketCodec, OversizedPayload) { /* payload > MTU → reject */ }
    TEST(PacketCodec, DuplicateSequence) { /* same seq # → deduplicated */ }
    ```
- **Why:** Unifies encoding/decoding logic; prevents bugs; protects against malformed/malicious packets
- **Dependencies:** NeuronCore (types)
- **Build Impact:** Very low (header-heavy)
- **Risk:** Low → Medium (error handling adds complexity; mitigated by unit tests)

#### Step 2.4: Server Main & Config Parsing
- **File:** Server/main.cpp, Config.cpp/h
- **Action:** Entry point: parse YAML config, initialize DebugTrace for logging, spin up server. Config loads tick_rate, max_players, db_url, port.
  ```cpp
  // Server/main.cpp
  int main(int argc, char* argv[]) {
    auto config = LoadConfig("server.yaml");
    InitLogging(config.log_level);
    Database db;
    db.Connect(config.database_url, config.db_pool_size);
    
    while (!g_shutdown_requested) {
      // Placeholder: empty tick
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    db.Disconnect();
    return 0;
  }
  ```
- **Why:** Launches server; config enables production deployment
- **Dependencies:** yaml-cpp, NeuronCore (DebugTrace/Fatal), Database
- **vcpkg:** `yaml-cpp` is already in `vcpkg.json`. Ensure `Server.vcxproj` has vcpkg manifest mode enabled.
- **Build Impact:** Low (new executables compile linking prior libs)
- **Risk:** Low (straightforward initialization)

#### Step 2.5: Skeletal Server Tick Loop
- **File:** NeuronServer/SimulationEngine.cpp/h
- **Action:** Implement 60 Hz tick loop (6 phases) as stubs. Currently does nothing except advance tick counter and measure time.
  ```cpp
  // NeuronServer/SimulationEngine.h
  class SimulationEngine {
    void Tick(uint64_t tick_num);  // Called every 16.67 ms
  private:
    void Phase1_InputProcessing() { /* stub */ }
    void Phase2_VoxelUpdate() { /* stub */ }
    void Phase3_Physics() { /* stub */ }
    void Phase4_Combat() { /* stub */ }
    void Phase5_Mining() { /* stub */ }
    void Phase6_Effects() { /* stub */ }
  };
  ```
- **Why:** Skeleton for later gameplay logic; verifies timing
- **Dependencies:** None yet (stubs)
- **Build Impact:** Very low (functions empty)
- **Risk:** Very low

#### Step 2.6: Server Network Transport (Socket Recv Loop)
- **File:** NeuronServer/SocketManager.cpp/h, updated server main loop
- **Action:** Create SocketManager that:
  - Binds Neuron::UDPSocket to 0.0.0.0:7777
  - Receives up to 100 datagrams per tick (non-blocking)
  - Validates & deserializes packets using packet codec
  - Enqueues validated commands
  ```cpp
  // NeuronServer/SocketManager.h
  class SocketManager {
    bool Init(uint16_t port);
    void RecvPackets(std::vector<ReceivedPacket>& outPackets);  // Called once per tick
  private:
    Neuron::UDPSocket m_socket;
    std::vector<uint8_t> m_recv_buffer;
  };
  ```
  - **WinSock2 specific:** Use WSAAsyncSelect or WSAEventSelect for non-blocking
  - Add to server main loop: `socketMgr.RecvPackets(packets);` before tick
- **Why:** Core I/O path; non-blocking design prevents tick stalls
- **Dependencies:** Neuron::UDPSocket, packet_codec, server logging
- **Build Impact:** Low (links ws2_32 on Windows)
- **Risk:** Medium (non-blocking I/O is tricky; test packet loss scenarios)

#### Step 2.7: Verify Build & Test Basic Server Start
- **File:** Build & run tests
- **Action:** Build server; run with config pointing to test SQL Server; verify:
  - Server logs "Initialized"
  - Tick loop runs at 60 Hz (logs tick count every 1 sec)
  - TCP/UDP port 7777 listens (use netstat)
  - Database connection succeeds (SELECT 1)
  - 🆕 **Tick-time histogram prints every 60 ticks** (min, p50, p95, p99, max)
- **Why:** Catch linkage & runtime issues early; establish performance baseline
- **Dependencies:** Steps 2.1–2.6
- **Build Impact:** Zero (verification)
- **Risk:** Low

#### 🆕 Step 2.8: Performance Measurement Infrastructure
- **File:** NeuronServer/TickProfiler.cpp/h
- **Action:** Add tick-time recording from day 1 (per validation recommendation):
  ```cpp
  class TickProfiler {
    void BeginTick();
    void EndTick();
    void PrintHistogram();  // Every 60 ticks: min, p50, p95, p99, max
    bool IsHealthy() const; // p99 < 16.67 ms for last 600 ticks
  private:
    std::vector<double> m_tick_times;  // Rolling window of 600 samples (10 sec)
  };
  ```
  - Integrated into SimulationEngine::Tick() wrapper
  - Log warning if p99 > 16.67 ms for > 10 consecutive ticks
  - Used as CI gate in later phases: fail build if p99 consistently over budget
- **Why:** Enables continuous performance monitoring; catches regressions early instead of Phase 6
- **Dependencies:** SimulationEngine (Step 2.5)
- **Build Impact:** Very low (timing instrumentation)
- **Risk:** Very low

**Deliverables:**
- ✅ Server binary compiles (Server.exe: tick loop + socket + DB)
- ✅ UDP socket listens on 7777 with WinSock2 (non-blocking via `ioctlsocket FIONBIO`)
- ✅ MS SQL Server connection pool initialized via ODBC (configurable pool size, slow query logging > 100 ms)
- ✅ Packet codec (de)serializes commands with CRC-32C (SSE4.2 hw) / magic / sequence validation
- ✅ Server logs via ServerLog (stdout/stderr) — `NeuronServer/ServerLog.h` (`LogInfo`, `LogWarn`, `LogError`, `LogFatal`)
- ✅ Config YAML loads correctly (`config/server.yaml`)
- 🆕 ✅ Tick-time histogram prints every 60 ticks (min, p50, p95, p99, max)
- 🆕 ✅ SocketManager per-sender sequence dedup (60-tick window) + rate-limited CRC logging (1/min per sender)
- 🆕 ✅ Windows `SetConsoleCtrlHandler` for proper CTRL_C / CTRL_SHUTDOWN shutdown
- 🆕 ✅ Release build configs fixed (PCH Create + AdditionalIncludeDirectories for all projects)
- ✅ Packet codec unit tests → completed in **Phase 2.5** (Microsoft Native Unit Test framework; `Tests.NeuronCore` project)

**Success Criteria:** ✅ MET
```bash
./Server --config config/server.yaml
# Output: "Server initialized. Listening on 0.0.0.0:7777"
# Tick count increments every 1 sec: "Tick 60, 120, 180, ..."
# netstat -an | grep 7777  shows LISTENING
# 🆕 Tick histogram: "Tick p50=0.02ms p95=0.05ms p99=0.08ms max=0.12ms"
```

**🆕 Phase 2 CI Gate:** ✅ ALL GATES PASSED
- ✅ Build succeeds on Windows x64 Debug (NeuronCore, NeuronServer, GameLogic, Server — 0 errors)
- ✅ Release build configs corrected (PCH + include dirs)
- ✅ Server starts and ticks for 10 sec without crash
- ✅ Unit tests: packet codec round-trip, CRC detection → completed in Phase 2.5

---

### Phase 2.5: Unit Test Project & Phase 1/2 Test Coverage ✅ COMPLETE

**Status:** Completed March 10, 2026. Tests.NeuronCore project added with 51 tests covering Phase 1 (Types, Constants, ChunkID, Vec3, AABB) and Phase 2 (PacketCodec round-trip, CRC corruption, magic mismatch, too-short/oversized). All tests pass via `vstest.console.exe`. Test count later grew to 82 with Phase 3 additions.

**Goal:** Add a Microsoft Native Unit Test project (`Tests.NeuronCore`) to the solution and implement unit tests covering all Phase 1 and Phase 2 deliverables. This closes the remaining Phase 2 CI gate item and establishes the test infrastructure for all subsequent phases.

**Test Framework:** [Microsoft Native Unit Test Framework](https://learn.microsoft.com/en-us/visualstudio/test/writing-unit-tests-for-c-cpp) (`#include <CppUnitTest.h>`) — ships with Visual Studio, zero external dependencies, integrates with VS Test Explorer, and runs headless via `vstest.console.exe` for CI.

#### Step 2.5.1: Create Test Project
- **File:** `Tests.NeuronCore/Tests.NeuronCore.vcxproj`, `Tests.NeuronCore/pch.h`, `Tests.NeuronCore/pch.cpp`
- **Action:** Add a Native Unit Test DLL project to the solution:
  - **Project type:** Dynamic Library (DLL) with `Microsoft.VisualStudio.TestTools.CppUnitTestFramework` NuGet or VS-provided lib
  - **References:** NeuronCore (static lib), NeuronServer (static lib)
  - **Include dirs:** `$(SolutionDir)NeuronCore;$(SolutionDir)NeuronServer;$(SolutionDir)tests`
  - **PCH:** `pch.h` includes `<CppUnitTest.h>` + `NeuronCore.h`
  - **Language standard:** `stdcpplatest` (Debug), `stdcpp20` (Release)
  - **Platform:** x64 only (matches solution)
- **Why:** Enables automated regression testing for all core libraries; VS Test Explorer provides IDE integration for run/debug
- **Dependencies:** NeuronCore.lib, NeuronServer.lib, ws2_32.lib
- **Build Impact:** Low (new DLL target; does not affect other projects)
- **Risk:** Very low (standard VS project template)

#### Step 2.5.2: Phase 1 Tests — Types & Constants
- **File:** `Tests.NeuronCore/TypesTests.cpp`
- **Action:** Port the Phase 1 verification checks into proper test methods:
  ```cpp
  using namespace Microsoft::VisualStudio::CppUnitTestFramework;

  TEST_CLASS(TypesTests)
  {
    TEST_METHOD(ConstantsAreCorrect)
    {
      Assert::AreEqual(60u, Neuron::TICK_RATE_HZ);
      Assert::AreEqual(32, Neuron::CHUNK_SIZE);
      Assert::AreEqual(16, Neuron::SECTOR_COUNT);
    }
    TEST_METHOD(ChunkIDEncoding)
    {
      auto id = Neuron::makeChunkID(1, 2, 3, 4, 5);
      Assert::AreNotEqual(Neuron::INVALID_CHUNK, id);
    }
    TEST_METHOD(Vec3Arithmetic) { /* +, -, *, += */ }
    TEST_METHOD(AABBContains) { /* contains() + overlaps() */ }
    TEST_METHOD(EnumSizes) { /* sizeof checks for EntityType, VoxelType, etc. */ }
  };
  ```
- **Why:** Replaces the removed `VerifyPhase1.exe` with a proper, repeatable test suite
- **Dependencies:** NeuronCore/Types.h, Constants.h
- **Risk:** Very low

#### Step 2.5.3: Phase 2 Tests — Packet Codec
- **File:** `Tests.NeuronCore/PacketCodecTests.cpp`
- **Action:** Test packet encode/decode, CRC validation, and error handling:
  ```cpp
  TEST_CLASS(PacketCodecTests)
  {
    TEST_METHOD(RoundTrip_CmdInput)
    {
      Neuron::CmdInput cmd{};
      cmd.playerId = 42;
      cmd.action = Neuron::ActionType::Move;
      cmd.targetX = 1.0f;
      auto bytes = Neuron::encodePacket(cmd, /*seq=*/1);
      Neuron::DecodedPacket out;
      auto result = Neuron::decodePacket(bytes, out);
      Assert::AreEqual((int)Neuron::DecodeResult::Ok, (int)result);
      Assert::AreEqual((uint16_t)sizeof(Neuron::CmdInput), out.header.payloadSize);
    }
    TEST_METHOD(RoundTrip_SnapState) { /* encode→decode SnapState */ }
    TEST_METHOD(RoundTrip_PingPacket) { /* encode→decode Ping */ }
    TEST_METHOD(CRCDetectsCorruption)
    {
      auto bytes = Neuron::encodePacket(Neuron::CmdInput{}, 1);
      bytes.back() ^= 0xFF; // corrupt last payload byte
      Neuron::DecodedPacket out;
      Assert::AreEqual((int)Neuron::DecodeResult::BadCrc, (int)Neuron::decodePacket(bytes, out));
    }
    TEST_METHOD(MagicMismatch)
    {
      auto bytes = Neuron::encodePacket(Neuron::CmdInput{}, 1);
      bytes[0] = 0x00; // corrupt magic
      Neuron::DecodedPacket out;
      Assert::AreEqual((int)Neuron::DecodeResult::BadMagic, (int)Neuron::decodePacket(bytes, out));
    }
    TEST_METHOD(TooShortPacket)
    {
      std::vector<uint8_t> tiny(4, 0);
      Neuron::DecodedPacket out;
      Assert::AreEqual((int)Neuron::DecodeResult::TooShort, (int)Neuron::decodePacket(tiny, out));
    }
    TEST_METHOD(OversizedPayload) { /* payloadSize > MAX_PACKET_SIZE → Oversized */ }
  };
  ```
- **Why:** Validates the wire protocol that all client↔server communication depends on
- **Dependencies:** NeuronCore/PacketCodec.h, PacketTypes.h
- **Risk:** Very low

**Deliverables:**
- ✅ `Tests.NeuronCore` project added to `StarStrike.slnx` (Native Unit Test DLL)
- ✅ Phase 1 tests: Types, Constants, ChunkID, Vec3, AABB (27 tests)
- ✅ Phase 2 tests: PacketCodec round-trip (all packet types), CRC corruption, magic mismatch, too-short, oversized (15 tests)
- ✅ CRC32 consistency tests (9 tests)
- ✅ All tests pass in VS Test Explorer and via `vstest.console.exe`

**Success Criteria:** ✅ MET
```bash
msbuild StarStrike.slnx /p:Configuration=Debug /p:Platform=x64
vstest.console.exe x64\Debug\Tests.NeuronCore.dll
# Total tests: 51 — Passed: 51 (0 failures)
```

**Phase 2.5 CI Gate:** ✅ ALL GATES PASSED
- ✅ Build succeeds (7 projects: existing 6 + Tests.NeuronCore)
- ✅ All unit tests pass: Types, PacketCodec, CRC32
- ✅ Test Explorer discovers and runs all test methods

---

### Phase 3: Entity System & Voxel Storage (Weeks 3.5–5, ~16 files) ✅ COMPLETE

**Status:** Completed March 10, 2026. All Phase 3 subsystems implemented: EntitySystem (ECS-lite AoS with free pool), VoxelSystem (chunk storage + RLE codec), Sector/SectorManager (4×4 grid), WorldManager (orchestrator), ChunkStore (persistence), config/schema.sql (MS SQL Server DDL). Server updated with world init, test entity spawning, and persistence flush. 31 new unit tests added (EntitySystem: 9, RLE codec: 7, VoxelSystem: 5, Sector: 10). Total test count: 82, all passing.

**Goal:** ECS-lite entity management, voxel chunk serialization, database schema loaded. Server spawns test entities.

> 🆕 **Validation additions:** Finalized DB schema with chunk locking, indices on foreign keys, transaction isolation semantics, partitioning strategy. Unit tests for EntityID free pool and RLE codec.

#### Step 3.1: ECS-Lite Entity System (Array-of-Structs)
- **File:** GameLogic/EntitySystem.cpp/h
- **Action:** Implement entity management:
  ```cpp
  // GameLogic/EntitySystem.h
  struct Entity {
    EntityID id;
    Vec3 pos, vel, accel; Quat rot;
    uint8_t type;  // EntityType enum
    uint16_t owner_player_id;
    uint32_t hp, max_hp;
    EntityID target_id;
    uint64_t last_update_tick;
  };
  
  class EntitySystem {
    EntityID SpawnEntity(const Entity& e);
    void DestroyEntity(EntityID id);
    Entity* GetEntity(EntityID id);
    void TickUpdate();  // Iterate all entities once (iterate vectors, not map)
  private:
    std::vector<Entity> m_entities;
    std::unordered_map<EntityID, size_t> m_entity_lookup;  // Fast lookup by ID
    std::vector<EntityID> m_free_pool;  // Reuse IDs to avoid ID exhaustion
  };
  ```
  - Emphasis: Contiguous arrays for cache coherency; one iteration per phase
  - No pointers between entities (use IDs + lookup map)
  - 🆕 **Unit tests (required before phase exit):**
    ```cpp
    TEST(EntitySystem, FreePoolCorrectness) {
      // Spawn 100K entities, destroy random subset, spawn again
      // Verify: no ID collision, all IDs valid, free pool drained correctly
    }
    TEST(EntitySystem, SpawnDestroyCycle) {
      // Spawn, verify GetEntity() returns valid ptr
      // Destroy, verify GetEntity() returns nullptr
      // Re-spawn, verify ID reused from free pool
    }
    ```
- **Why:** High-performance entity iteration (critical for 60 Hz @ 50 players)
- **Dependencies:** NeuronCore (EntityID, Vec3, Quat)
- **Build Impact:** Low (new translation unit)
- **Risk:** Low (straightforward dynamic array management)

#### Step 3.2: Voxel Chunk Structure & Serialization
- **File:** GameLogic/VoxelSystem.cpp/h
- **Action:** Implement chunk storage and RLE serialization:
  ```cpp
  // GameLogic/VoxelSystem.h
  struct VoxelChunk {
    Vec3i min;                       // Min corner in world coords
    uint8_t voxels[32][32][32];     // 32 KB per chunk
    bool dirty;
    uint64_t version;
    uint64_t modified_tick;
  };
  
  class VoxelSystem {
    void SetVoxel(const Vec3i& world_pos, uint8_t type);
    uint8_t GetVoxel(const Vec3i& world_pos) const;
    std::vector<uint8_t> SerializeChunk(const VoxelChunk& chunk) const;
    VoxelChunk DeserializeChunk(const std::vector<uint8_t>& data) const;
    
  private:
    std::unordered_map<ChunkID, VoxelChunk> m_chunks;
    std::vector<VoxelDelta> m_delta_buffer;  // Flushed to DB periodically
  };
  
  // RLE codec (inside .cpp)
  // Serialize: iterate voxels; emit (count, type) runs
  // Deserialize: read (count, type) pairs; expand back to grid
  ```
  - Serialization achieves ~4–6 KB per chunk (rocky terrain ≈ 75% empty)
  - Paired with zstd compression for additional 20% reduction if needed
  - 🆕 **Unit tests (required before phase exit):**
    ```cpp
    TEST(RLECodec, RoundTripSparse) {
      // 1 voxel in otherwise empty 32³ chunk → serialize → deserialize → byte-by-byte compare
    }
    TEST(RLECodec, RoundTripDense) {
      // All voxels filled (worst case) → serialize → deserialize → compare
    }
    TEST(RLECodec, RoundTripHollowSphere) {
      // Boundary case: shell of solid voxels around empty interior
    }
    TEST(RLECodec, EmptyChunk) {
      // All EMPTY → minimal output (1 run)
    }
    ```
- **Why:** Efficient storage; enables fast chunk swapping on server/client
- **Dependencies:** NeuronCore (Vec3i, types)
- **Build Impact:** Low (new translation unit)
- **Risk:** Medium (RLE codec has edge cases; unit test thoroughly with sparse/dense chunks)

#### Step 3.3: MS SQL Server Schema Creation
- **File:** config/schema.sql
- **Action:** Create tables for voxel_chunks, voxel_events, players, ships, sectors. Include indices on chunk_id, sector_id, player_id.
  ```sql
  IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'voxel_chunks')
  CREATE TABLE voxel_chunks (
    chunk_id VARBINARY(8) NOT NULL PRIMARY KEY,
    sector_id VARCHAR(10),
    voxel_data VARBINARY(MAX),
    version INT DEFAULT 1,
    modified_at DATETIME2 DEFAULT GETUTCDATE(),
    locked_by_player_id INT DEFAULT NULL,
    lock_expiry_tick BIGINT DEFAULT NULL
  );
  CREATE INDEX idx_chunks_sector ON voxel_chunks(sector_id);

  IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'voxel_events')
  CREATE TABLE voxel_events (
    id BIGINT IDENTITY(1,1) PRIMARY KEY,
    chunk_id VARBINARY(8) NOT NULL,
    world_x INT, world_y INT, world_z INT,
    old_type SMALLINT, new_type SMALLINT,
    player_id INT,
    tick_number BIGINT,
    created_at DATETIME2 DEFAULT GETUTCDATE()
  );
  CREATE INDEX idx_voxel_events_chunk_tick ON voxel_events(chunk_id, tick_number);

  IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'players')
  CREATE TABLE players (
    player_id INT IDENTITY(1,1) PRIMARY KEY,
    username VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(128) NOT NULL,
    last_login DATETIME2
  );

  IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'ships')
  CREATE TABLE ships (
    ship_id INT IDENTITY(1,1) PRIMARY KEY,
    owner_id INT NOT NULL REFERENCES players(player_id),
    pos_x REAL, pos_y REAL, pos_z REAL,
    hp INT, max_hp INT,
    cargo_json NVARCHAR(MAX),
    last_saved_tick BIGINT
  );
  CREATE INDEX idx_ships_owner ON ships(owner_id);
  ```
  - 🆕 **Partitioning strategy (per validation):** Consider partition functions and schemes on voxel_chunks for 4 partitions (one per sector row) if table exceeds 100K rows
  - 🆕 **Transaction semantics for concurrent chunk edits:**
    ```sql
    -- Atomically update chunk + append event (prevents lost updates)
    BEGIN TRANSACTION;
      UPDATE voxel_chunks SET voxel_data = @data, version = version + 1,
             locked_by_player_id = NULL
       WHERE chunk_id = @chunkId AND locked_by_player_id = @playerId;
      INSERT INTO voxel_events (chunk_id, world_x, world_y, world_z,
             old_type, new_type, player_id, tick_number)
       VALUES (@chunkId, @x, @y, @z, @oldType, @newType, @playerId, @tick);
    COMMIT;
    ```
  - 🆕 **Isolation level:** Use `READ COMMITTED` (default) for most queries; `SERIALIZABLE` only for chunk version-check updates to prevent lost writes from concurrent miners
- **Why:** Defines data model; allows server to persist world state; prevents concurrent write issues
- **Dependencies:** MS SQL Server 2019+ (or SQL Server Express / LocalDB for development)
- **Build Impact:** Zero (data only, run `sqlcmd -S localhost -d starstrike -i config\schema.sql`)
- **Risk:** Low → Medium (concurrent writes; mitigated by locking fields + versioned updates)

#### Step 3.4: Chunk Persistence Layer (Load/Save to DB)
- **File:** NeuronServer/ChunkStore.cpp/h
- **Action:** Wrap Database class; implement:
  ```cpp
  // NeuronServer/ChunkStore.h
  class ChunkStore {
    VoxelChunk LoadChunk(ChunkID id);
    void SaveChunk(const VoxelChunk& chunk);
    void FlushDirtyChunks();  // Called every 30 sec
    void AppendVoxelEvent(const VoxelDelta& delta);
  private:
    Database& m_db;
    std::vector<VoxelDelta> m_event_buffer;  // Batch inserts
  };
  ```
  - LoadChunk: SELECT & decompress RLE data
  - SaveChunk: INSERT or UPDATE with version check
  - FlushDirtyChunks: batch UPDATE all dirty chunks
  - AppendVoxelEvent: buffer deltas; flush via batch INSERT every 1 sec
- **Why:** Abstracts SQL details; centralizes persistence logic
- **Dependencies:** Database, VoxelSystem
- **Build Impact:** Low (new translation unit, uses ODBC via Database layer)
- **Risk:** Medium (transaction isolation; test concurrent writes)

#### Step 3.5: Sector Manager
- **File:** GameLogic/Sector.cpp/h
- **Action:** Manage 4×4 grid of sectors; each sector owns ~4,096 chunks.
  ```cpp
  // GameLogic/Sector.h
  class Sector {
    Vec3i GetBounds() const;  // Min/max corners
    ChunkID WorldPosToChunkID(const Vec3i& world_pos) const;
    bool IsInBounds(const Vec3i& world_pos) const;
  };
  
  class SectorManager {
    Sector& GetSector(int grid_x, int grid_y);
    void Init(int grid_width, int grid_height);
  private:
    std::vector<Sector> m_sectors;  // 16 for MVP
  };
  ```
- **Why:** Partitions world; enables interest-based culling later
- **Dependencies:** NeuronCore (Vec3i, ChunkID)
- **Build Impact:** Very low (mostly logic)
- **Risk:** Low

#### Step 3.6: World Manager (Top-Level Orchestrator)
- **File:** GameLogic/WorldManager.cpp/h
- **Action:** Central point for entity, voxel, chunk management:
  ```cpp
  class WorldManager {
    void Init(const WorldConfig& cfg);
    void Tick();  // Called by SimulationEngine each tick
    
    EntitySystem& GetEntitySystem() { return m_entity_system; }
    VoxelSystem& GetVoxelSystem() { return m_voxel_system; }
    SectorManager& GetSectorManager() { return m_sector_mgr; }
    ChunkStore& GetChunkStore() { return m_chunk_store; }
  private:
    EntitySystem m_entity_system;
    VoxelSystem m_voxel_system;
    SectorManager m_sector_mgr;
    ChunkStore m_chunk_store;
  };
  ```
- **Why:** Single coherent interface; simplifies server main loop coupling
- **Dependencies:** EntitySystem, VoxelSystem, SectorManager, ChunkStore
- **Build Impact:** Very low (delegation)
- **Risk:** Very low

#### Step 3.7: Test Entity & Chunk Initialization
- **File:** Server/main.cpp (updated)
- **Action:** In server main loop, after WorldManager::Init():
  - Load all voxel_chunks from DB into memory
  - Spawn 1 test asteroid per sector (EntityType::ASTEROID)
  - Spawn 1 test ship (EntityType::SHIP) at sector (0,0)
  - Log entity IDs and chunk counts
- **Why:** Verifies WorldManager initialization; catches database loading issues
- **Dependencies:** Steps 3.1–3.6
- **Build Impact:** Zero (logic changes only)
- **Risk:** Low

**Deliverables:**
- ✅ EntitySystem (ECS-lite array-of-structs) implemented
- ✅ VoxelSystem with RLE serialization
- ✅ MS SQL Server schema created (voxel_chunks, voxel_events, players, ships — with indices & locking)
- ✅ ChunkStore persistence layer
- ✅ SectorManager for world partitioning
- ✅ WorldManager orchestrator
- ✅ Server loads & spawns test entities

**🆕 Phase 3 Unit Tests (required before phase exit):**
- ✅ EntitySystem free pool correctness (spawn 100K, destroy random, verify no ID collision)
- ✅ RLE codec round-trip (sparse, dense, hollow sphere, empty — byte-by-byte compare)
- ✅ ChunkStore CRUD with versioned updates (load, save, flush, verify version increments)

**🆕 Phase 3 CI Gate:** ✅ ALL GATES PASSED
- ✅ Unit tests pass: EntitySystem (9) + RLE codec (7) + VoxelSystem (5) + Sector (10) — 31 new tests, 82 total
- ✅ Build succeeds on Windows x64 Debug (0 errors, 7 projects)
- ✅ Server initializes WorldManager, spawns 16 test asteroids + 1 test ship

**Success Criteria:** ✅ MET
```bash
./Server --config config/server.yaml
# Logs:
# "WorldManager initialized (4 x 4 sectors)"
# "Spawned test asteroid 0 at (256, 256, 128)"
# ... (16 asteroids, 1 per sector)
# "Spawned test ship 16 at (0, 0, 0)"
# "World ready: 17 entities, 0 chunks"  (chunks loaded from DB if connected)
# Tick counter increments
# vstest.console.exe: Total tests: 82, Passed: 82
```

---

### Phase 4: Client Foundation & Input (Weeks 5–7, ~20 files)

**Goal:** Client connects to server, receives and displays entities. Input processing skeleton. Still no voxel rendering yet.

> **Phase 1.5 overlap:** Steps 4.1 (DX12 Device) and 4.6 (Client Main Loop) are **already complete** — implemented during Phase 1.5 as `GraphicsCore` and `ClientEngine`/`GameApp` respectively. The planned `DX12Device.cpp/.h` became `GraphicsCore.cpp/.h` with a more comprehensive API. The planned `StarStrike/main.cpp` is `StarStrike/WinMain.cpp` with the loop in `ClientEngine::Run()`. Remaining Phase 4 work: Steps 4.2–4.5 (networking, entity cache, input system, camera).

> 🆕 **Validation additions:** Client↔server integration test (packet round-trip), 50-player load test setup.

#### Step 4.1: DirectX 12 Device Initialization ✅ COMPLETE (Phase 1.5)
- **File:** NeuronClient/GraphicsCore.cpp/h *(was planned as DX12Device.cpp/h)*
- **Action:** Implement DX12 device, command queue, swap chain:
  ```cpp
  // NeuronClient/DX12Device.h (now GraphicsCore.h)
  class DX12Device {
    bool Initialize(HWND hwnd, uint32_t width, uint32_t height);
    void BeginFrame();
    ID3D12GraphicsCommandList* GetCommandList() { return m_cmd_list; }
    void Present();
    
    ID3D12Device* GetDevice() { return m_device.Get(); }
    ID3D12CommandQueue* GetQueue() { return m_cmd_queue.Get(); }
  private:
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_cmd_queue;
    ComPtr<IDXGISwapChain4> m_swap_chain;
    ComPtr<ID3D12Resource> m_render_targets[2];  // Double-buffered
    ComPtr<ID3D12DescriptorHeap> m_rtv_heap;
  };
  ```
  - Feature level 12_1 minimum
  - Debug layer enabled in Debug builds
  - Fence synchronization for present
  - 🆕 **Contingency steps:**
    - If swap chain creation fails: log detailed DXGI error code + adapter name; prompt user to update drivers
    - If feature level 12_1 unavailable: fall back to 12_0 with reduced shader model
- **Why:** Mandatory for rendering; establishes GPU command pipeline
- **Dependencies:** d3d12.lib, dxgi.lib, directxmath.lib (Windows SDK)
- **Build Impact:** Medium (DirectX SDK linking)
- **Risk:** Medium (GPU driver issues; log diagnostic info on failure)

#### Step 4.2: Client Network Socket & Snapshot Decoder
- **File:** NeuronClient/ClientSocket.cpp/h, SnapshotDecoder.cpp/h
- **Action:** Client-side UDP socket; deserialize snapshots:
  ```cpp
  // NeuronClient/ClientSocket.h
  class ClientSocket {
    bool Connect(const std::string& server_addr, uint16_t port);
    bool SendCommand(const CmdInput& cmd);
    bool RecvSnapshot(SnapshotState& snapshot);  // Non-blocking
  private:
    Neuron::UDPSocket m_socket;
    std::vector<uint8_t> m_recv_buffer;
  };
  
  // NeuronClient/SnapshotDecoder.h
  struct SnapshotState {
    uint64_t tick;
    std::vector<EntityDelta> entity_deltas;
    std::vector<VoxelDelta> voxel_deltas;
  };
  bool DeserializeSnapshot(const std::vector<uint8_t>& bytes, SnapshotState& snap);
  ```
  - Deserialize entity deltas (pos, rot, vel, hp, despawn)
  - Deserialize voxel deltas (world pos + type)
- **Why:** Client receives server state; core gameplay loop dependency
- **Dependencies:** Neuron::UDPSocket, packet_codec, NeuronCore types
- **Build Impact:** Low (new translation units)
- **Risk:** Medium (packet format mismatches between server/client codecs)

#### Step 4.3: Client Entity Cache (Local AoS Storage)
- **File:** NeuronClient/EntityCache.cpp/h
- **Action:** Mirror server entity array on client (for rendering):
  ```cpp
  // NeuronClient/EntityCache.h
  struct ClientEntity {
    EntityID id;
    Vec3 pos, vel, rotation;
    uint8_t type;
    uint16_t hp, max_hp;
    uint64_t last_snapshot_tick;
  };
  
  class EntityCache {
    void UpdateFromSnapshot(const std::vector<EntityDelta>& deltas);
    void DestroyEntity(EntityID id);
    const std::vector<ClientEntity>& GetAll() const { return m_entities; }
  private:
    std::vector<ClientEntity> m_entities;
    std::unordered_map<EntityID, size_t> m_lookup;
  };
  ```
  - Apply deltas from snapshots
  - Remove despawned entities
  - Smooth motion via linear interpolation (client prediction)
- **Why:** Enables rendering of all visible entities
- **Dependencies:** NeuronCore (EntityID, Vec3)
- **Build Impact:** Very low (straightforward arrays)
- **Risk:** Low

#### Step 4.4: Client Input System (Keyboard/Mouse)
- **File:** NeuronClient/InputSystem.cpp/h
- **Action:** Capture Win32 keyboard/mouse events; translate to commands:
  ```cpp
  // NeuronClient/InputSystem.h
  class InputSystem {
    void ProcessWindowMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    std::vector<CmdInput> GetPendingCommands();  // Called once per frame
  private:
    bool m_keys_pressed[256];  // Key states
    Vec2 m_mouse_pos, m_mouse_delta;
  };
  
  // NeuronClient/CommandTranslator.h
  CmdInput TranslateInputToCommand(const InputState& input);
  // WASD -> move commands; click -> target; R -> attack, M -> mine
  ```
  - Queue up to 60 commands/sec (one per frame)
  - Each command includes target position or entity ID
- **Why:** Core gameplay loop feedback; links player input to server state
- **Dependencies:** Windows.h, client event system
- **Build Impact:** Very low (Win32 message handling)
- **Risk:** Low

#### Step 4.5: Client Camera Controller
- **File:** NeuronClient/Camera.cpp/h
- **Action:** Isometric camera; pan (WASD/mouse drag) and zoom (mouse wheel):
  ```cpp
  // NeuronClient/Camera.h
  class Camera {
    void Update(const InputState& input, float dt);
    Mat4x4 GetViewMatrix() const;
    Mat4x4 GetProjectionMatrix() const;
    
    void Pan(Vec2 delta);
    void Zoom(float factor);  // Mouse wheel
    void LookAt(Vec3 target);  // Optional: follow entity
  private:
    Vec3 m_pos;  // Camera position
    float m_yaw = 45.0f, m_pitch = 45.0f;  // Fixed isometric angles
    float m_zoom = 1.0f;   // 4x to 64x magnification
  };
  ```
  - Fixed pitch/yaw (isometric); only pan and zoom change
  - Frustum culling: only render chunks within view bounds
- **Why:** Allows player to see the world; critical for usability
- **Dependencies:** DirectXMath, input_system, NeuronCore (Vec3, Mat4x4)
- **Build Impact:** Very low
- **Risk:** Low

#### Step 4.6: Client Main Loop & Networking Integration ✅ COMPLETE (Phase 1.5)
- **File:** StarStrike/WinMain.cpp, StarStrike/GameApp.cpp/h, NeuronClient/NeuronClient.cpp/h *(was planned as StarStrike/main.cpp)*
- **Action:** Implement game app class:
  ```cpp
  class GameApp {
    bool Initialize(const std::string& server_addr, uint16_t port);
    void Frame(float delta_time);  // Called at 60 FPS
    bool IsRunning() const;
    
  private:
    DX12Device m_dx12;
    ClientSocket m_socket;
    EntityCache m_entity_cache;
    InputSystem m_input;
    Camera m_camera;
    
    uint64_t m_local_tick = 0;
  };
  
  // StarStrike/main.cpp
  int main() {
    GameApp app;
    app.Initialize("127.0.0.1", 7777);
    
    while (app.IsRunning()) {
      app.Frame(frame_delta);  // ~16 ms @ 60 FPS
    }
    return 0;
  }
  ```
  - Main loop:
    1. Process OS messages (input)
    2. Serialize & send commands to server
    3. Recv snapshot (non-blocking)
    4. Update entity cache
    5. Update camera
    6. Render (placeholder clear screen)
    7. Present
- **Why:** Unified entry point; orchestrates all client subsystems
- **Dependencies:** All prior client components
- **Build Impact:** Low (executable)
- **Risk:** Medium (main loop timing is critical; jitter = visual artifacts)

#### Step 4.7: Verify Client Connectivity & Input Relay
- **File:** Integration test/manual test
- **Action:** Run server & client; verify:
  - Client connects without error (no "connection refused")
  - Keyboard input captured and logged (WASD key presses)
  - Snapshot received (non-blocking; log tick numbers)
  - Entity cache updates (log entity position changes)
  - Camera responds to mouse wheel (zoom changes)
  - DX12 window renders solid color (swap chain present works)
- **Why:** Catch network/input/rendering integration issues before moving to graphics
- **Dependencies:** Steps 4.1–4.6
- **Build Impact:** Zero (testing)
- **Risk:** Low

**Deliverables:**
- ✅ DirectX 12 device initialized, swap chain created
- ✅ Client connects to server via UDP
- ✅ Snapshots received and entity cache updated
- ✅ Keyboard/mouse input captured
- ✅ Camera (pan/zoom) responds to input
- ✅ Main game loop runs at 60 FPS

**🆕 Phase 4 Integration Tests (required before phase exit):**
- ✅ Client↔Server packet round-trip: client sends CmdInput, server receives & validates, server sends snapshot, client deserializes
- ✅ 50-player load test setup: spawn 10 ghost clients (reduced count for CI), verify tick time < 16.67 ms
- ✅ DX12 device initializes (CI gate: window opens, swap chain presents solid color)

**🆕 Phase 4 CI Gate:**
- Integration tests pass: client↔server round-trip, DX12 device init
- Build succeeds on Windows x64 (Debug + Release)
- Server + 10 ghost clients: p99 tick < 16.67 ms for 60 sec

**Success Criteria:**
```bash
./Server
./StarStrike --server 127.0.0.1:7777
# Window opens (solid color)
# Log: "Connected to server tick 123"
# Log: "Snapshot received, 2 entities"
# Log: "Input: W pressed"
# Camera zooms in/out with mouse wheel
```

---

### Phase 5: Rendering & Gameplay Systems (Weeks 7–10, ~24 files)

**Goal:** Voxel rendering (greedy mesh), entity sprites, basic HUD. Combat, mining, voxel destruction working end-to-end.

> 🆕 **Validation additions:** Detailed greedy mesh algorithm with face removal, normal encoding, seam handling, and mesh cache invalidation. Shader compilation MSBuild integration with offline precompilation fallback. Snapshot builder with periodic full-state resend for packet loss resilience.

#### Step 5.1: Greedy Mesh Generation
- **File:** NeuronClient/VoxelRenderer.cpp/h (includes mesh generation)
- **Action:** Implement greedy meshing algorithm:
  ```cpp
  // NeuronClient/VoxelRenderer.h
  struct MeshVertex {
    int16_t x, y, z;       // Local coords
    uint8_t normal_packed; // Normal (octahedron-encoded)
    uint8_t voxel_type;    // Material/color lookup
    uint8_t ao;            // Ambient occlusion 0–3
  };
  
  struct ChunkMesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint16_t> indices;
    AABB bounds;
    uint64_t cached_hash;
  };
  
  class VoxelRenderer {
    ChunkMesh GenerateMesh(const VoxelChunk& chunk);
    // Greedy algorithm:
    // For each axis (X, Y, Z):
    //   Scan plane-by-plane
    //   Extend rectangles greedily
    //   Output quad (4 verts + 2 tri indices)
  };
  ```
  - Result: ~40,000 triangles per chunk (vs. 520,000 naive)
  - Octahedron-encode normals to 1 byte (6 directions)
  - Cache mesh hash; regenerate only on chunk dirty flag
  - 🆕 **Detailed greedy mesh algorithm (per validation):**
    ```cpp
    // Pseudocode:
    for (axis in {X, Y, Z}) {
      for (layer in layers[axis]) {
        // Flatten 2D layer: check face exposure
        is_covered[y][z] = (GetVoxel(layer, y, z) != EMPTY && 
                            GetVoxel(layer-1, y, z) == EMPTY);  // Face exposed
        // Greedy rectangles
        for (y = 0; y < 32; ++y)
          for (z = 0; z < 32; ++z)
            if (!rect_allocated[y][z] && is_covered[y][z])
              // Find max height h and width w of same-type rectangle
              Quad quad = { {layer, y, z}, {layer, y+h, z+w} };
              Mark rect_allocated[y..y+h][z..z+w];
        // Encode quads as 2 triangles, encode normal from axis direction
      }
    }
    ```
  - 🆕 **Normal encoding:** 6 cardinal directions mapped to 3-bit index (0–5); stored as `uint8_t normal_packed`
  - 🆕 **Seam handling at chunk boundaries:**
    - Each chunk mesh is independent; sample 1-voxel border from adjacent chunks when determining face visibility
    - When adjacent chunk is dirty, mark both chunks for re-mesh
    - Unit test: render 2×2 adjacent chunks, verify no visible cracks between boundaries
  - 🆕 **Mesh cache invalidation:**
    - On chunk dirty flag: defer re-mesh to next frame (Option B)
    - Queue limit: max 10 async mesh rebuilds per frame (prevents stall if > 5 chunks change simultaneously)
    - If queue full: defer remaining to subsequent frames
  - 🆕 **Unit tests (required before phase exit):**
    ```cpp
    TEST(GreedyMesh, SparseChunk) { /* 1 voxel → 6 faces → 12 triangles */ }
    TEST(GreedyMesh, DenseChunk) { /* all solid → only outer faces */ }
    TEST(GreedyMesh, HollowSphere) { /* boundary case: inner + outer faces */ }
    TEST(GreedyMesh, ChunkBoundarySeam) { /* 2×2 chunks, verify no cracks */ }
    TEST(GreedyMesh, Performance) { /* 512 chunks mesh generation, verify < 1ms each */ }
    ```
- **Why:** Critical for performance; enables rendering of large voxel worlds
- **Dependencies:** NeuronCore, voxel data structure
- **Build Impact:** Medium (complex algorithm, may take time to optimize)
- **Risk:** High (greedy mesh is finicky; off-by-one errors common; extensive unit test)

#### Step 5.2: VB/IB Creation & GPU Upload
- **File:** NeuronClient/MeshPool.cpp/h
- **Action:** Allocate vertex/index buffers on GPU; upload meshes:
  ```cpp
  class MeshPool {
    struct ChunkMeshGPU {
      ComPtr<ID3D12Resource> vertex_buffer;
      ComPtr<ID3D12Resource> index_buffer;
      uint32_t index_count;
      D3D12_VERTEX_BUFFER_VIEW vbv;
      D3D12_INDEX_BUFFER_VIEW ibv;
    };
    
    ChunkMeshGPU UploadMesh(const ChunkMesh& mesh, DX12Device& device);
    void EvictOldestChunkMesh();  // LRU when memory constrained
  private:
    std::unordered_map<ChunkID, ChunkMeshGPU> m_chunk_meshes;
    size_t m_total_vram_mb = 0;
    static const size_t MAX_VRAM_MB = 512;  // Limit GPU VRAM usage
  };
  ```
  - Use D3D12 copy queue for async upload (non-blocking, one per-frame batch)
  - LRU eviction when pool exhausted
- **Why:** Separates CPU mesh generation from GPU resource management; enables non-blocking mesh uploads
- **Dependencies:** DX12Device, ChunkMesh
- **Build Impact:** Low (GPU resource management)
- **Risk:** Medium (GPU memory fragmentation; test with 200+ chunks loaded)

#### Step 5.3: Voxel Shaders (HLSL)
- **File:** StarStrike/shaders/voxel.hlsl (vertex + pixel)
- **Action:** Implement voxel rendering shaders:
  ```hlsl
  // Vertex Shader
  cbuffer SceneConstants : register(b0) {
    float4x4 view_proj;  // Pre-multiplied view * projection
    float3 camera_pos;
    float padding;
  };
  
  struct VS_INPUT {
    int3 pos : POSITION;        // Local voxel coords
    uint normal_packed : NORMAL;
    uint voxel_type : TEXCOORD0;
    uint ao : TEXCOORD1;
  };
  
  struct VS_OUTPUT {
    float4 pos_clip : SV_POSITION;
    float3 normal_world : NORMAL;
    uint voxel_type : TEXCOORD1;
    uint ao : TEXCOORD2;
  };
  
  VS_OUTPUT main(VS_INPUT input, uint instance_id : SV_InstanceID) {
    // Unpack normal
    float3 normal = UnpackNormal(input.normal_packed);
    
    // Apply chunk instance offset
    float3 chunk_offset = GetChunkOffset(instance_id);
    float3 pos_world = chunk_offset + float3(input.pos);
    
    // Project
    VS_OUTPUT output;
    output.pos_clip = mul(float4(pos_world, 1.0), view_proj);
    output.normal_world = normal;
    output.voxel_type = input.voxel_type;
    output.ao = input.ao;
    return output;
  }
  
  // Pixel Shader
  Texture2D tex_terrain : register(t0);
  Texture2D tex_emissive_mask : register(t1);
  
  float4 main(VS_OUTPUT input) : SV_TARGET {
    // Fetch terrain color
    float2 uv = GetTerrainUV(input.voxel_type);
    float3 color_base = tex_terrain.Sample(sampler0, uv).rgb;
    
    // Simple lighting: normal · sun_direction
    float3 sun_dir = normalize(float3(0.7, 0.7, 0.5));
    float diffuse = max(0.2, dot(input.normal_world, sun_dir));
    
    // AO: reduce brightness based on corner occlusion
    float ao = 1.0 - (input.ao * 0.1);
    diffuse *= ao;
    
    // Emissive (neon fissures)
    float is_emissive = tex_emissive_mask.Sample(sampler0, float2(input.voxel_type / 256.0, 0.5)).r;
    float3 emissive_color = color_base * is_emissive * 2.0;
    
    // Output
    float3 final = color_base * diffuse + emissive_color;
    return float4(final, 1.0);
  }
  ```
  - Input layout matches MeshVertex struct
  - Textures: single 2048×2048 atlas (diffuse, normal, emissive)
  - Lighting: single directional (sun) + ambient
- **Why:** Renders voxel geometry; essential for visual feedback
- **Dependencies:** DirectXMath, shared unpack/utility functions
- **Build Impact:** Low (shader compilation via FXC or DXC)
- **Risk:** Medium (shader compilation errors; test with debug layer enabled)

#### Step 5.4: Compile Shaders & Create PSO
- **File:** NeuronClient/ShaderCompiler.cpp/h (calls FXC/DXC)
- **Action:** Compile HLSL shaders to bytecode; create graphics pipeline state object:
  ```cpp
  class ShaderCompiler {
    std::vector<uint8_t> CompileShader(const std::string& hlsl_file, 
                                       const std::string& entry_point,
                                       const std::string& target);
     // target = "vs_6_0", "ps_6_0", etc.
  };
  
  // In DX12Device:
  void CreateVoxelPSO() {
    auto vs_bytes = ShaderCompiler::Compile("voxel.hlsl", "main", "vs_6_0");
    auto ps_bytes = ShaderCompiler::Compile("voxel.hlsl", "main", "ps_6_0");
    
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = { 
      .pRootSignature = m_root_sig.Get(),
      .VS = { vs_bytes.data(), vs_bytes.size() },
      .PS = { ps_bytes.data(), ps_bytes.size() },
      .RasterizerState = { .CullMode = D3D12_CULL_MODE_BACK, ... },
      .DepthStencilState = { .DepthEnable = TRUE, ... },
      // ...
    };
    m_device->CreateGraphicsPipelineState(&pso_desc, &m_pso_voxel);
  }
  ```
  - Use FXC.exe (MSVC toolset) or DXC (Shader Model 6.0+)
  - Fallback to pre-compiled bytecode if shader compilation fails in CI
  - 🆕 **Shader compilation MSBuild integration (per validation):**
    - Add HLSL files to `StarStrike.vcxproj` as `<FxCompile>` items with `ShaderType`, `EntryPointName`, and `ShaderModel` metadata
    - MSBuild compiles shaders automatically during build; output `.cso` files copied to output directory
    ```xml
    <!-- StarStrike/StarStrike.vcxproj -->
    <FxCompile Include="shaders\voxel_vs.hlsl">
      <ShaderType>Vertex</ShaderType>
      <ShaderModel>6.0</ShaderModel>
      <EntryPointName>VS_Main</EntryPointName>
    </FxCompile>
    <FxCompile Include="shaders\voxel_ps.hlsl">
      <ShaderType>Pixel</ShaderType>
      <ShaderModel>6.0</ShaderModel>
      <EntryPointName>PS_Main</EntryPointName>
    </FxCompile>
    ```
  - 🆕 **Offline precompilation fallback:**
    - `tools/compile_shaders.bat`: invokes FXC → outputs `.h` header with bytecode array constant
    - Pre-compiled bytecode checked in at `tools/shader_bytecode/`
    - MSBuild: if FXC not found, fall back to precompiled `.h` includes
    - CI: use precompiled bytecode (FXC may not be available on all runners)
- **Why:** Separates compilation (offline) from runtime (fast pipeline creation)
- **Dependencies:** DX12Device, HLSL compiler
- **Build Impact:** Medium (FXC invocation via MSBuild `<FxCompile>`; fallback to precompiled bytecode)
- **Risk:** Medium (compiler availability; mitigate via precompiled blobs)

#### Step 5.5: Server-Side Combat & Mining Commands
- **File:** GameLogic/CombatSystem.cpp/h, MiningSystem.cpp/h
- **Action:** Implement command processing for attack & mine:
  ```cpp
  // GameLogic/CombatSystem.h
  class CombatSystem {
    void ProcessAttackCommand(const CmdInput& cmd, WorldManager& world);
    void ProcessHits(WorldManager& world);  // Physics phase
    
  private:
    struct ProjectileEvent {
      EntityID projectile_id;
      EntityID target_id;
      Vec3 impact_pos;
    };
    std::vector<ProjectileEvent> m_hit_events;
  };
  
  // CombatSystem implementation (in .cpp):
  // Phase 4: Combat
  void CombatSystem::ProcessHits(WorldManager& world) {
    auto& entities = world.GetEntitySystem().GetAll();
    for (auto& proj : entities | filter_type(PROJECTILE)) {
      AABB proj_bounds = GetAABB(proj.pos);
      for (auto& ship : entities | filter_type(SHIP)) {
        if (AABBOverlap(proj_bounds, GetAABB(ship.pos))) {
          ship.hp -= PROJECTILE_DAMAGE;
          m_hit_events.push_back({proj.id, ship.id, proj.pos});
        }
      }
    }
  }
  
  // GameLogic/MiningSystem.h
  class MiningSystem {
    void ProcessMiningCommand(const CmdInput& cmd, WorldManager& world);
    void ExtractMiningVoxels(WorldManager& world);  // Phase 5
  private:
    std::vector<VoxelDelta> m_mined_voxels;  // Add to world delta buffer
  };
  ```
  - Attack: spawn projectile; next frame raycasts detect hits
  - Mine: raycast from ship → voxel; remove voxel if minable type
  - Broadcast deltas in next snapshot
- **Why:** Core gameplay mechanics; ensure they work before moving to network replication
- **Dependencies:** EntitySystem, VoxelSystem, Physics (raycasts)
- **Build Impact:** Low (new subsystems)
- **Risk:** Medium (hit detection edge cases; test with high velocities)

#### Step 5.6: Integrate Combat/Mining into Tick Phases
- **File:** NeuronServer/SimulationEngine.cpp (update Phase 4 & 5)
- **Action:** Hook CombatSystem & MiningSystem into tick loop:
  ```cpp
  void SimulationEngine::Tick() {
    Phase1_InputProcessing();    // Validate commands
    Phase2_VoxelUpdate();        // Apply queued voxel changes
    Phase3_Physics();            // Move ships, projectiles
    Phase4_Combat();             // CombatSystem::ProcessHits()
    Phase5_Mining();             // MiningSystem::ExtractMiningVoxels()
    Phase6_Effects();            // Spawn FX, cleanup
  }
  ```
- **Why:** Ensures phases execute in correct order (input → physics → combat)
- **Dependencies:** CombatSystem, MiningSystem, SimulationEngine
- **Build Impact:** Zero (integration only)
- **Risk:** Low

#### Step 5.7: Snapshot Builder & Delta Encoding
- **File:** NeuronServer/SnapshotBuilder.cpp/h
- **Action:** Encode entity & voxel deltas for efficient network transmission:
  ```cpp
  // NeuronServer/SnapshotBuilder.h
  class SnapshotBuilder {
    SnapshotState BuildSnapshot(uint64_t tick, const WorldManager& world);
    // Returns: entity deltas + voxel deltas (only changed fields)
  };
  
  // Encoding:
  // For each entity:
  //   u32 entity_id
  //   u8 field_mask (bit 0=pos, 1=rot, 2=vel, 3=hp, 4=despawn)
  //   [optional fields based on mask]
  // For each voxel delta:
  //   i32 world_x, world_y, world_z
  //   u8 new_voxel_type
  ```
  - Track entity state last-sent; only encode if changed
  - Quantize floats (position to cm precision, rotation to 16-bit)
  - Voxel deltas: only send chunks within interest radius
  - 🆕 **Packet loss resilience (per validation):**
    ```
    Snapshot sequence: Full(N), Delta+1, Delta+2, ..., Delta+(N-1), Full(2N), ...
    - Send full entity state every N frames (N = 60 = 1 sec)
    - Between full snapshots: send delta-only (changed fields)
    - Client: use most recent full snapshot + all deltas received since
    - On missed full snapshot: request resend via RESEND_FULL packet type
    ```
  - 🆕 **Unit tests:**
    ```cpp
    TEST(SnapshotBuilder, DeltaEncodingCorrect) {
      // Entity pos change → delta has pos bit set, rot/hp bits clear
    }
    TEST(SnapshotBuilder, FullStateResend) {
      // After N ticks, full snapshot includes all entity fields
    }
    ```
- **Why:** Minimizes bandwidth; critical for scale to 50 players
- **Dependencies:** EntitySystem, VoxelSystem, interest radius calculation
- **Build Impact:** Low (new subsystem)
- **Risk:** Low (delta encoding is well-established; unit test with various entity states)

#### Step 5.8: Client Entity Rendering & HUD
- **File:** NeuronClient/EntityRenderer.cpp/h, NeuronClient/HUD.cpp/h
- **Action:** Render ships & asteroids; display HUD elements:
  ```cpp
  // Entity rendering: simple colored cubes or quads
  // HUD: health bar, ammo counter, radar (minimap), chat log
  // For MVP: 2D overlays on top of 3D scene
  ```
  - Entity rendering: quad sprites or simple box meshes
  - HUD: D2D1 text rendering (ship name, HP, cargo count)
  - Radar: 2D minimap of nearby entities (512-unit radius)
- **Why:** Visual feedback for player; essential for usability
- **Dependencies:** DX12Device, entity data, D2D1 (for 2D text)
- **Build Impact:** Low (new subsystems)
- **Risk:** Low

**Deliverables:**
- ✅ Greedy mesh generation (40,000 tri/chunk)
- ✅ VB/IB upload with LRU eviction
- ✅ Voxel shaders (vertex + pixel) compiled
- ✅ Graphics PSO created
- ✅ Combat system (projectiles, hit detection, damage)
- ✅ Mining system (voxel extraction, resource spawn)
- ✅ Entity rendering (ships, asteroids as sprites)
- ✅ HUD display (health, cargo, radar)

**Success Criteria:**
```bash
./Server
./StarStrike
# Voxel chunks render (colored cubes visible)
# 2 ships visible (player's and test asteroid)
# Click to move; ship interpolates smoothly
# WASD to attack/mine; voxels disappear; resources spawn
# HUD shows health/cargo
```

---

### Phase 6: Persistence, Optimization & Polish (Weeks 10–13, ~18 files)

**Goal:** Full data persistence, stress testing @ 50 players, observability, final polish.

#### Step 6.1: Player Authentication & Ship Loading
- **File:** NeuronServer/PlayerStore.cpp/h
- **Action:** Player login/logout with ship state persistence:
  ```cpp
  class PlayerStore {
    bool Login(const std::string& username, const std::string& password, uint32_t& out_player_id);
    std::vector<Ship> LoadShips(uint32_t player_id);
    void SaveShips(uint32_t player_id, const std::vector<Ship>& ships);
    void Logout(uint32_t player_id);
  };
  ```
  - Query players table; verify password hash
  - LoadShips: fetch from ships table; reconstruct entity
  - SaveShips: UPDATE ships table (position, HP, cargo, etc.)
  - Called on player join (load) and every 5 ticks (incremental save)
- **Security:** Password verification must use a slow hash (bcrypt or Argon2id) — never store or compare plaintext passwords. Consider DTLS or a challenge–response handshake over the UDP transport to prevent credential sniffing. Rate-limit login attempts to mitigate brute-force attacks.
- **Why:** Players can persist their progress across sessions
- **Dependencies:** Database, WorldManager
- **Build Impact:** Low (new translation unit)
- **Risk:** Medium (transaction isolation; test concurrent logins)

#### Step 6.2: Voxel Event Flushing & Recovery
- **File:** NeuronServer/ChunkStore.cpp (enhanced), TransactionLog.cpp/h
- **Action:** Periodic voxel delta flushing; on-crash recovery:
  ```cpp
  // In ChunkStore:
  void FlushVoxelDeltas() {
    if (m_event_buffer.empty()) return;
    
    // Batch INSERT voxel_events
    stringstream sql;
    sql << "INSERT INTO voxel_events (chunk_id, world_x/y/z, old/new_type, player_id, tick_number) VALUES ";
    for (const auto& delta : m_event_buffer) {
      sql << "(" << encode(delta) << "), ";
    }
    m_db.Execute(sql.str());
    m_event_buffer.clear();
  }
  
  // On server restart:
  void RecoverFromLog(Database& db, WorldManager& world) {
    // For each chunk:
    //   Load voxel_chunks (latest snapshot)
    //   Replay voxel_events since last snapshot
    //   Reconstruct final state
  }
  ```
  - FlushVoxelDeltas called every 1 sec (60 ticks)
  - FlushDirtyChunks called every 30 sec
  - Recovery on server start: reload chunks + replay events
- **Why:** Ensures world persistence; no lost destructions on crash
- **Dependencies:** Database, ChunkStore, VoxelSystem
- **Build Impact:** Low (existing database layer)
- **Risk:** Medium (transaction ordering; concurrent writers can cause splits; use SERIALIZABLE isolation)

#### Step 6.3: Interest Management (Visibility Culling)
- **File:** NeuronServer/Visibility.cpp/h
- **Action:** Per-player entity culling; only send deltas for visible entities:
  ```cpp
  class VisibilityManager {
    std::vector<EntityID> GetVisibleEntities(const Entity& player_ship, const WorldManager& world);
    // Entities within D (16 chunks ≈ 512 units): always included
    // Entities within 2D: lower update frequency (every 2 snapshots)
    // Entities beyond 2D: not sent
  };
  ```
  - Recompute per-player every 200 ms (12 ticks)
  - Cache results until next update
  - Voxel deltas: only send chunks in streaming radius (8 chunks from camera)
- **Why:** Reduces bandwidth 4x for 50 players; enables larger player counts
- **Dependencies:** EntitySystem, SectorManager
- **Build Impact:** Low (new calculation)
- **Risk:** Low (distance culling is straightforward)

#### Step 6.4: Observability: Logging & Metrics
- **File:** NeuronServer/Logger.cpp/h, Metrics.cpp/h
- **Action:** Structured logging + Prometheus metrics:
  ```cpp
  // Logging (DebugTrace / Fatal from NeuronCore/Debug.h)
  DebugTrace("Player {} joined (ships: {})\n", player_id, ship_count);
  DebugTrace("WARNING: Tick {} took {:.2f} ms (budget: 16.67)\n", tick, tick_ms);
  Fatal("Database query failed: {}", error_msg);
  
  // Metrics
  Metrics::Counter("server_tick_count", 1);
  Metrics::Histogram("server_tick_duration_ms", tick_duration_ms);
  Metrics::Gauge("network_connections_active", connection_count);
  Metrics::Counter("voxel_destruction_events_total", voxel_deltas.size());
  Metrics::Histogram("db_query_duration_ms", query_ms);
  ```
  - Logs to file (rotation: 100 MB per file, 10 backups)
  - Metrics exposed via HTTP endpoint (localhost:9090/metrics) in Prometheus text format
- **Why:** Enables production monitoring and debugging
- **Dependencies:** NeuronCore (DebugTrace/Fatal), Prometheus client library
- **Build Impact:** Low (logging integration)
- **Risk:** Low (well-established libraries)

#### Step 6.5: Server Health Check Endpoint
- **File:** NeuronServer/Health.cpp/h
- **Action:** HTTP endpoint for /health and /ready (Kubernetes-compatible):
  ```cpp
  struct ServerHealth {
    bool is_running;
    uint64_t current_tick;
    int active_players;
    float tick_time_ms;
    float bandwidth_kbps;
    
    bool IsHealthy() const {
      return is_running && tick_time_ms < 20.0f && 
             last_tick_time_ms < 60000;  // No tick for 60 sec = unhealthy
    }
  };
  
  // Exposed:
  // GET /health  -> 200 OK { "status": "healthy" } or 503
  // GET /ready   -> 200 OK when accepting players
  ```
  - Integrated into main tick loop (adds negligible overhead)
  - Enables Kubernetes liveness/readiness probes
- **Why:** Production deployment requires health monitoring
- **Dependencies:** Simple HTTP server (embed or use microserver lib)
- **Build Impact:** Low (HTTP handling)
- **Risk:** Low

#### Step 6.6: Post-Processing Pipeline (Bloom + Pixelation)
- **File:** StarStrike/shaders/bloom_extract.hlsl, blur.hlsl, composite.hlsl
- **Action:** Implement glow effect:
  1. Render scene to low-res RT (640×360)
  2. Extract bright pixels (bloom mask)
  3. Blur (4-level pyramid)
  4. Composite with original
  5. Nearest-neighbor upsample to 1920×1080
  ```hlsl
  // Bloom extraction
  float brightness = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
  return brightness > 1.0 ? color : float4(0, 0, 0, 1);
  
  // Blur (separable, 4-tap)
  // Composite: additive blend glow
  ```
- **Why:** Retro aesthetic (glowing neon fissures); visual polish
- **Dependencies:** Voxel rendering pipeline
- **Build Impact:** Low (new shaders)
- **Risk:** Low

#### Step 6.7: Stress Test & Performance Profiling
- **File:** Test harness: tools/stress_test.cpp
- **Action:** Simulate 50 concurrent players:
  ```cpp
  // Ghost bot clients:
  // - Connect to server
  // - Send random MOVE/ATTACK/MINE commands at realistic rates
  // - Recv snapshots
  // - Track: tick duration, bandwidth, entity count, errors
  ```
  - Measure: server tick time, CPU usage, memory, network I/O
  - Target: tick < 16.67 ms @ 100% CPU (50 players)
  - Log results to CSV for graphing
  - 🆕 **Quantitative performance test harness (per validation):**
    ```cpp
    // tools/perf_test.cpp
    ServerInstance server;
    server.SpawnPlayers(50);  // Spawns AI ghost clients
    for (int i = 0; i < 3600; ++i) {  // 60 sec @ 60 Hz
      uint64_t tick_start = PerfCounter();
      server.Tick();
      double tick_ms = (PerfCounter() - tick_start) / 1e6;
      tick_times.push_back(tick_ms);
    }
    PrintHistogram(tick_times);
    assert(Percentile(tick_times, 99) < 16.67);
    ```
  - 🆕 **Memory / GPU profiling checklist (before shipping):**
    - Run with PIX GPU profiler; capture frame; verify < 200 chunks visible
    - Run with Windows Performance Analyzer; verify server < 4 GB heap peak
    - Run with GPU memory profiler; verify client < 1 GB VRAM (out of 2 GB budget)
    - Run with ASAN (Address Sanitizer); verify zero memory leaks
- **Why:** Validates performance targets; catches scaling issues early
- **Dependencies:** Ghost client library, server executable
- **Build Impact:** Medium (test harness + metrics aggregation)
- **Risk:** Medium (reproduction of production load is hard; ghost clients may be unrealistic)

#### Step 6.8: Windows Server Core Container Deployment Test
- **File:** Dockerfile, docker-compose.yaml
- **Action:** Build and test containerized deployment on Windows Server Core:
  ```dockerfile
  # Dockerfile
  FROM mcr.microsoft.com/windows/servercore:ltsc2022
  WORKDIR C:/StarStrike
  COPY x64/Release/Server/ .
  EXPOSE 7777/udp 8080/tcp
  ENTRYPOINT ["Server.exe"]
  ```
  ```powershell
  # Build & run
  msbuild StarStrike.slnx /p:Configuration=Release /p:Platform=x64
  docker build -t starstrike-server:latest .
  docker-compose up --detach

  # Verify
  docker exec starstrike-server powershell Test-NetConnection -ComputerName localhost -Port 7777
  Invoke-RestMethod http://localhost:8080/health   # 200 OK
  docker logs starstrike-server                     # Verify logging
  ```
  - Verify: server starts inside Windows Server Core container, DB connection succeeds, port 7777 listens
  - Test graceful shutdown: `docker stop` → `CTRL_SHUTDOWN_EVENT` → clean exit within 5 sec
  - Register `SetConsoleCtrlHandler` for `CTRL_C_EVENT` / `CTRL_SHUTDOWN_EVENT` in server main
- **Why:** Production-ready containerized deployment on Windows Server Core
- **Dependencies:** Docker Desktop (Windows containers mode), Windows Server Core 2022 base image, MS SQL Server for Windows
- **Build Impact:** Low (container tooling + deployment scripting)
- **Risk:** Low (standard Windows container patterns)

#### Step 6.9: Final Bug Fixes & Polish
- **File:** Throughout codebase
- **Action:** 
  - Playtesting (40+ hours with multiple players)
  - Bug triage & fixes
  - Performance tuning (profile hot paths; optimize mesh generation, database queries)
  - UI polish (font, color scheme, tooltips)
  - Documentation (README, BUILD.md, API docs)
- **Why:** MVP quality bar
- **Dependencies:** All prior phases complete
- **Build Impact:** Variable (bug-dependent)
- **Risk:** High (unknown unknowns; schedule buffer)

**Deliverables:**
- ✅ Player login/ship persistence
- ✅ Voxel event log with recovery
- ✅ Interest-based entity culling
- ✅ Structured logging + Prometheus metrics
- ✅ Health check endpoint
- ✅ Post-processing (bloom, pixelation)
- ✅ Stress test @ 50 concurrent players
- ✅ Windows Server Core containerized deployment
- ✅ Documentation & API examples

**Success Criteria:**
```bash
# Server @ 50 ghost players:
# "Tick 3600 took 15.2 ms (budget: 16.67)"  ✅
# Prometheus metrics scrape: tick count increments ✅
# Health check: /health returns 200 OK ✅
# Container deploy: docker-compose up → Windows Server Core container ready ✅
```

---

### 🆕 Phase 6.5: Audio & UI Placeholder (Week 14, ~8 files)

**Goal:** Basic audio feedback and UI elements for MVP polish. This phase is **post-core-gameplay** and can be partially deferred if schedule is tight.

> Added per IMPLEMENTATION_VALIDATION.md recommendation (Issue #7: Audio & UI Systems Missing).

#### Step 6.5.1: Audio Integration
- **File:** NeuronClient/AudioSystem.cpp/h
- **Action:** Integrate audio engine (NeuronClient::Sound if available, or minimal DirectSound wrapper):
  ```cpp
  class AudioSystem {
    void Init();
    void PlaySFX(SoundID id, Vec3 world_pos);  // 3D positional audio
    void PlayMusic(const std::string& track);   // Background loop
    void SetListenerPos(Vec3 pos);              // Camera position
  };
  ```
  - Sound events: weapon fire, projectile impact, mining drill loop, ship engine hum
  - Music: single ambient track (placeholder .wav)
  - Volume config via YAML
- **Why:** Audio feedback dramatically improves player immersion and gameplay readability
- **Dependencies:** DirectSound or XAudio2, NeuronCore
- **Build Impact:** Low (new subsystem)
- **Risk:** Low (audio is non-blocking; failures just produce silence)

#### Step 6.5.2: Main Menu & UI Panels
- **File:** NeuronClient/MainMenu.cpp/h, ResourcePanel.cpp/h, TargetInfo.cpp/h
- **Action:** Implement basic UI elements:
  ```cpp
  // Main Menu: Connect to Server, Settings, Quit
  // In-Game HUD:
  //   - Resource bar (top): minerals, credits
  //   - Target info (bottom-left): selected entity name, HP, distance
  //   - Minimap (bottom-right): entity dots in 512-unit radius
  //   - Chat log (bottom-center): last 5 messages
  ```
  - Use D2D1 text rendering (already available from Phase 5.8)
  - Keyboard shortcuts: ESC = menu, Tab = scoreboard, Enter = chat
- **Why:** Players need UI to understand game state; main menu enables server selection
- **Dependencies:** DX12Device, D2D1, InputSystem
- **Build Impact:** Low (UI overlay)
- **Risk:** Low

**Deliverables:**
- ✅ Weapon fire, impact, mining SFX play
- ✅ Background music loops
- ✅ Main menu (connect, settings, quit)
- ✅ In-game HUD (resources, target info, minimap, chat)

**Success Criteria:**
```bash
# Audio: weapon fire produces sound; impact audible at correct position
# Music: ambient track loops without gap
# Menu: ESC opens main menu; can reconnect to server
# HUD: resource count updates when mining; target info shows selected ship HP
```

---

## Build & Verification Strategy

### Build Configurations

| Config | Purpose | Compiler | Flags |
|--------|---------|----------|-------|
| **x64-debug** | Development | MSVC v145 | /Od /Zi /D_DEBUG |
| **x64-release** | Production server | MSVC v145 | /O2 /Oi /GL |
| **x64-release-client** | Shipping client | MSVC v145 | /O2 /GL /NDEBUG |

### Per-Phase Build Verification

**Phase 1:** `msbuild StarStrike.slnx /p:Configuration=Debug /p:Platform=x64 /t:NeuronCore` → no errors
**Phase 2:** `msbuild StarStrike.slnx /p:Configuration=Debug /p:Platform=x64 /t:Server` → executable links; runs without crash
**Phase 3:** `Server --config config/server.yaml` → loads DB, spawns test entities, ticks
**Phase 4:** `StarStrike --server 127.0.0.1:7777` → window opens, receives snapshots
**Phase 5:** Kill server, respawn; verify voxel destruction persists across restart
**Phase 6:** `docker-compose up && Invoke-RestMethod http://localhost:9090/metrics` → metrics exposed

### Incremental Build Strategy

- **PCH optimization:** After Phase 1, rebuilds skip common headers (60–70% faster)
- **Avoid wide headers:** Use forward declarations in .h files; include only in .cpp
- **Parallel compilation:** `msbuild /m` or Visual Studio parallel project build (utilize all cores)
- **Separate client/server builds:** If only server code changed, client rebuild skipped

### Testing Strategy

**Framework:** [Microsoft Native Unit Test](https://learn.microsoft.com/en-us/visualstudio/test/writing-unit-tests-for-c-cpp) (`CppUnitTest.h`). Test DLL projects per library, discovered by VS Test Explorer and `vstest.console.exe`. Tests live in `Tests.NeuronCore/` (and future `Tests.GameLogic/` etc. as needed).

| Phase | Unit Tests | Integration Tests | Manual Tests | CI Gate |
|-------|-----------|-------------------|--------------|--------|
| 1 | Types, constants (15+ static_asserts) | (none) | (none) | Build succeeds, StarStrike.exe links cleanly |
| 2 | 🆕 Packet codec (round-trip, CRC, dedup, magic) | Server connects, receives packet | netstat 7777 | 🆕 Unit tests pass, server ticks 10 sec |
| 2.5 | 🆕 Types/Constants, PacketCodec (all types), CRC32 | (none) | (none) | 🆕 `vstest.console.exe Tests.NeuronCore.dll` — 0 failures |
| 3 | 🆕 EntityID free pool (100K spawn/destroy), RLE codec (sparse/dense/hollow/empty) | Load chunks, save to DB | Server startup prints entities | 🆕 Unit tests pass, 256 chunks loaded |
| 4 | Input mapping, camera | 🆕 Client↔Server packet round-trip, DX12 device init | Keyboard input logged | 🆕 Integration tests pass, 10 ghost clients |
| 5 | 🆕 Greedy mesh (sparse/dense/hollow/seam/perf), 🆕 snapshot delta encoding | Voxels removed, entities hit | Play 30 min gameplay | 🆕 Mesh tests pass, shader compiles |
| 6 | Persistence, visibility | Crash & recover, 50 players | 🆕 Stress test 6 hrs, 🆕 perf harness CSV | 🆕 p99 tick < 16.67 ms @ 50 players |
| 6.5 | (none) | Audio plays, UI renders | Main menu navigable | Build succeeds |

### 🆕 CI/CD Pipeline Strategy (per validation)

**Platform:** GitHub Actions (Windows runner) or Azure Pipelines

| Phase | CI Requirements |
|-------|----------------|
| 1–2 | Unit tests pass, build succeeds on Windows x64 (Debug + Release) |
| 3 | Database integration tests (create schema, load/save chunks — use local SQL Server Express / LocalDB) |
| 4+ | DX12 rendering test (requires GPU-enabled CI runner or local testing) |
| 6 | 50-player load test: p99 tick < 16.67 ms average (CI runs reduced 10-player version; full 50 run manually) |

**CI Environment:**
- Windows runner with MSVC v145 toolset and GPU (or skip DX12 rendering tests in headless CI)
- Shader precompilation happens offline (Phase 5.4); CI uses precompiled bytecode
- SQL Server Express (or LocalDB) for database tests
- Load test runs with reduced player count (10 instead of 50; scales linearly if tick time < 16.67 ms)

---

## Data & Content Dependencies

### Assets Needed

| Asset | Format | Size | Created By | Milestone |
|-------|--------|------|-----------|-----------|
| Terrain atlas | PNG RGBA | 2048×2048 | Artist | Phase 5 (or placeholder) |
| Emissive mask | PNG (1-bit) | 2048×2048 | Artist | Phase 6 |
| Ship models | Simple quads/cubes | — | Programmer | Phase 5 (2D sprites) |
| Voxel types | Enum (64 types) | Code | Designer | Phase 1 |
| Ship stats (YAML) | Config | — | Designer | Phase 4 |
| Weapon params (YAML) | Config | — | Designer | Phase 5 |

### Procedural Generation (Optional for MVP)

- Initial world: generate 4×4 sectors × 16 chunks = 256 chunks (flat plane or noise-based)
- Seeded RNG: reproducible worlds
- Saved to voxel_chunks table on first server run

---

## Risks & Mitigations

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|-----------|
| **Mesh generation correctness** | Graphics corrupted, unplayable | High | Unit tests (sparse/dense chunks); visual inspection |
| **Network packet loss** | Freezing, desync | Medium | Client resend, server deduplication, sequence numbers |
| **Voxel delta flood** | Bandwidth overrun, lag | Medium | Interest radius culling, delta aggregation, RLE compression |
| **Server tick > 16.67 ms** | Jitter, unplayable | High | Profiling, optimize hot paths (entity iteration), consider thread pool |
| **Database contention** | Stalls, connection pool exhaustion | Medium | Connection pooling, batch writes, async queries, profile query times |
| **GPU memory fragmentation** | Crashes under load | Low | LRU eviction, defragmentation, test with 1000+ chunks |
| **WinSock2 API mismatch** | Connection refused, packet corruption | Low | Unit tests for socket codec, test on Windows 10/11/Server Core |
| **Schedule slip** | Ship late, unfinished features | Medium | Regular playtesting, prioritize critical-path features (network, persistence, rendering), cut scope if needed |

### 🆕 Concrete Risk Mitigation Checklists (per validation)

**Phase 5.1 — Greedy Mesh:**
- [ ] Unit tests: sparse chunk (1 voxel), dense chunk (all voxels), hollow sphere (boundary), 2×2 chunk seam
- [ ] Visual test: render known geometry, screenshot comparison against reference image
- [ ] Performance: measure mesh generation time for 512 chunks; plot histogram; all < 1 ms

**Phase 4.1 — DX12 Device Initialization:**
- [ ] If shader compilation fails at runtime: load pre-compiled bytecode blob from `tools/shader_bytecode/`
- [ ] If swap chain creation fails: log detailed DXGI error code + adapter info; prompt user to update drivers
- [ ] If feature level 12_1 unavailable: fall back to 12_0 with reduced shader model

**Phase 6.7 — 50-Player Stress Test:**
- [ ] Run for 6 hours continuously; log tick histogram every minute
- [ ] Verify: p99 tick < 16.67 ms, zero crashes, zero memory leaks (ASAN)
- [ ] If tick budget exceeded: profile with PIX/WPA, identify top 3 hotspots, optimize before shipping

**Phase 3.3 — Database Concurrency:**
- [ ] Test: two clients mine same chunk simultaneously; verify no lost writes (version check)
- [ ] Test: connection pool exhaustion (pool size = 2, 10 concurrent queries); verify graceful degradation
- [ ] Monitor: log slow queries (> 100 ms); alert if > 10 slow queries per minute

---

## Success Criteria (MVP)

### Functional
- [ ] 50 concurrent players connect without errors
- [ ] Ships move smoothly (< 50 ms latency, client prediction)
- [ ] Combat: fire projectile → hit ship → damage applied visible to all players
- [ ] Mining: extract voxel → removed on all clients, resource spawns
- [ ] Persistence: restart server → world state unchanged (voxels, ships, resources)
- [ ] Chat: one player's message visible to all in-sector

### Performance
- [ ] Server tick: ≤ 16.67 ms @ 60 Hz (100% CPU @ 50 players)
- [ ] 🆕 Server tick p99 < 16.67 ms (measured via TickProfiler histogram over 6-hour stress test)
- [ ] Network bandwidth: ≤ 1.5 KB/s inbound, ≤ 14 KB/s outbound per player
- [ ] Client frame rate: ≥ 60 FPS (mid-range GPU; 1920×1080)
- [ ] Voxel mesh generation: < 1 ms per chunk (cached, LOD'd)
- [ ] Database latency: < 30 ms for query, < 100 ms for write
- [ ] 🆕 Server memory: < 4 GB heap peak (measured via WPA / ASAN)
- [ ] 🆕 Client VRAM: < 1 GB (measured via GPU memory profiler)

### Deployment
- [ ] Server builds as Release x64 binary (< 5 min)
- [ ] Windows Server Core container image builds (< 10 min)
- [ ] docker-compose up → server + SQL Server running (< 30 sec)
- [ ] Server health check: /health responds 200 OK
- [ ] Prometheus metrics: tick count, player count, bandwidth exposed
- [ ] Graceful shutdown: docker stop → CTRL_SHUTDOWN_EVENT → clean exit in < 5 sec, no data loss

### Code Quality
- [ ] Zero compiler warnings (MSVC /WX)
- [ ] No memory leaks (ASAN, Dr. Memory, or CRT debug heap)
- [ ] Static analysis: MSVC /analyze, cppcheck pass
- [ ] Code follows CODE_STANDARDS.md (naming, formatting, memory management)

---

## Manual Checklist

Use this to track progress:

```
## Phase 1 (Weeks 1–1.5) ✅ COMPLETE (March 5, 2026)
- [x] MSBuild solution configured (`StarStrike.slnx`, per-project `.vcxproj`)
- [x] vcpkg manifest created (`vcpkg.json`: cppwinrt, winpixevent, yaml-cpp)
- [x] NeuronCore Types.h, Constants.h created (full API with 15+ static_asserts verified)
- [x] PCH headers configured (`NeuronCore/pch.h` + MSBuild `<PrecompiledHeader>` settings)
- [x] x64-debug builds successfully (MSVC 19.50, 0 errors)
- [x] Neuron platform stubs (Socket, FileSystem, Threading, Timer)
- [x] VerifyPhase1.exe compiled and ran (exit code 0); target later replaced by StarStrike.exe in Phase 1.5

## Phase 1.5 (Week 1.5–2) ✅ COMPLETE (March 6, 2026)
- [x] NeuronCore STATIC library (6 .cpp) with PCH — math, timers, file I/O, debug, async, events, packet codec, socket
- [x] NeuronClient STATIC library (11 .cpp) with PCH — full DX12 pipeline
- [x] GraphicsCore: D3D12 device, swap chain, command queue/list, fence sync, debug layer
- [x] DescriptorHeap: CPU + GPU descriptor allocation (CBV/SRV/UAV/Sampler/RTV/DSV)
- [x] RootSignature: builder pattern with constants, CBV, SRV, UAV, static samplers
- [x] PipelineState: GraphicsPSO wrapper (depth, blend, rasterizer, input layout, topology)
- [x] ResourceStateTracker: batched resource barriers (up to 16), split barrier support
- [x] GpuBuffer / GpuResource: RAII D3D12 resource wrappers with state tracking
- [x] SamplerManager: hash-based sampler descriptor deduplication
- [x] DDSTextureLoader: DDS file/memory → D3D12 resource (mipmaps, cubemaps)
- [x] ClientEngine (NeuronClient.cpp): Win32 window, message loop, GameMain lifecycle
- [x] GameMain.h: abstract base with lifecycle hooks (Update, RenderScene, RenderCanvas)
- [x] GameApp: concrete GameMain — language detection, touch input, WndProc
- [x] WinMain: entry point → ClientEngine::Startup → GameApp → Run → Shutdown
- [x] Per-target PCH via MSBuild `<PrecompiledHeader>` settings
- [x] NuGet: CppWinRT + Windows App SDK; vcpkg: winpixevent + yaml-cpp
- [x] UNICODE/WIN64 builds enforced
- [x] MSBuild migration complete: CMake removed, `.vcxproj` + `.slnx` in place
- [x] Phase 2 boilerplate scaffolded (NeuronServer: Database, SimulationEngine, SocketManager, TickProfiler; Server: main, Config)

## Phase 2 (Weeks 2–3.5) ✅ COMPLETE (March 8, 2026)
- [x] Neuron::UDPSocket (WinSock2) implemented — non-blocking, bind/sendTo/recvFrom/close
- [x] MS SQL Server connection pool working via ODBC (configurable pool size, slow query logging > 100 ms)
- [x] Packet codec serializes/deserializes all message types (CmdInput, CmdChat, CmdRequestChunk, SnapState, Ping)
- [x] 🆕 Packet framing: magic (0x53535452), CRC-32C (SSE4.2 hw), sequence number, error handling
- [x] Server binary runs, tick loop at 60 Hz (Server.exe links and runs)
- [x] Port 7777 listens (SocketManager binds + receives)
- [x] 🆕 Tick-time histogram prints every 60 ticks (min, p50, p95, p99, max via TickProfiler)
- [x] 🆕 SocketManager: per-sender sequence dedup (60-tick window) + rate-limited CRC logging (1/min per sender)
- [x] 🆕 Windows SetConsoleCtrlHandler replaces POSIX signals for proper shutdown
- [x] 🆕 Release build configs fixed: PCH Create for Release on pch.cpp + AdditionalIncludeDirectories for all projects
- [x] 🆕 config/server.yaml created (default server configuration)
- [x] 🆕 Unit tests: packet round-trip, CRC, dedup, magic mismatch → completed in Phase 2.5

## Phase 2.5 — Unit Test Project ✅ COMPLETE (March 10, 2026)
- [x] Tests.NeuronCore project added to solution (Native Unit Test DLL)
- [x] Phase 1 tests: Types, Constants, ChunkID, Vec3, AABB (27 tests)
- [x] Phase 2 tests: PacketCodec round-trip (CmdInput, SnapState, Ping), CRC corruption, magic mismatch, too-short, oversized (15 tests)
- [x] Phase 2 tests: CRC32 consistency (9 tests)
- [x] All 51 tests pass in VS Test Explorer and via vstest.console.exe

## Phase 3 (Weeks 3.5–5) ✅ COMPLETE (March 10, 2026)
- [x] EntitySystem (AoS) working; entities spawn & despawn
- [x] 🆕 EntityID free pool unit test (100K spawn/destroy, no ID collision)
- [x] VoxelSystem RLE codec tested (sparse & dense chunks)
- [x] 🆕 RLE round-trip tests: sparse, dense, hollow sphere, empty chunk, alternating types
- [x] MS SQL Server schema created, indexed (with locking fields & foreign key indices)
- [x] ChunkStore CRUD working (load/save/flush with versioned updates)
- [x] SectorManager 4×4 grid with bounds checking and world-to-chunk mapping
- [x] WorldManager orchestrator (owns EntitySystem + VoxelSystem + SectorManager)
- [x] Server initializes WorldManager, spawns 16 test asteroids + 1 test ship
- [x] Phase 3 unit tests: 31 new tests (EntitySystem: 9, RLE: 7, VoxelSystem: 5, Sector: 10)
- [x] Total test count: 82 (all passing)

## Phase 4 (Weeks 5–7)
- [x] DX12 device initializes, window opens *(done in Phase 1.5 as GraphicsCore)*
- [x] Client main loop runs at 60 FPS *(done in Phase 1.5 as ClientEngine::Run)*
- [ ] Client connects to server, receives snapshots
- [ ] Keyboard input captured & sent as commands
- [ ] Camera pan (WASD) & zoom (wheel) working
- [ ] Entity cache displays 2+ entities
- [ ] 🆕 Client↔Server integration test: packet round-trip verified
- [ ] 🆕 10 ghost clients: p99 tick < 16.67 ms for 60 sec

## Phase 5 (Weeks 7–10)
- [ ] Greedy mesh generation produces correct geometry
- [ ] 🆕 Mesh unit tests: sparse, dense, hollow, seam, performance
- [ ] VB/IB upload non-blocking; LRU eviction working
- [ ] Voxel shaders compile (FXC or DXC; 🆕 MSBuild `<FxCompile>` integration + offline fallback)
- [ ] PSO created, voxels render on screen
- [ ] Combat: projectile hits ship, damage applied
- [ ] Mining: voxels removed, visible to all players
- [ ] HUD displays health, cargo, radar
- [ ] 🆕 Snapshot builder: periodic full-state resend for packet loss resilience

## Phase 6 (Weeks 10–13)
- [ ] Player login/logout saves ship state to DB
- [ ] Voxel events flushed every 1 sec
- [ ] Crash recovery: restart server, voxel deltas replayed
- [ ] 50-player stress test: tick < 16.67 ms (🆕 quantitative perf harness with CSV output)
- [ ] 🆕 Memory/GPU profiling: server < 4 GB heap, client < 1 GB VRAM
- [ ] Observability: logs + Prometheus metrics working
- [ ] Windows Server Core container builds & runs
- [ ] 40+ hours playtesting, critical bugs fixed
- [ ] MVP release: docker-compose up → server ready for public test

## 🆕 Phase 6.5 (Week 14)
- [ ] Audio: weapon fire, impact, mining SFX play
- [ ] Background music loops
- [ ] Main menu: connect, settings, quit
- [ ] In-game HUD: resources, target info, minimap, chat
```

---

## 🆕 Phase Gate Validation Checklist (per validation)

Before each phase starts, verify:
- [ ] Test requirements (unit + integration) written for the phase
- [ ] Success criteria quantified (not just "logs", but "tick time < 16.67 ms")
- [ ] Risk mitigation steps concrete (not vague "extensive testing")
- [ ] Dependencies on prior phases met
- [ ] Estimated effort realistic (5–7 files per week is typical for experienced developer)
- [ ] Build system changes tested on Windows x64 (CI when available)

## 🆕 Missing Subsystems & Future Phases (Post-MVP)

For **Phase 7+**, consider:
- **Fog of War** (client-side culling based on radar)
- **Diplomacy & Alliances** (multiplayer coordination)
- **Save/Load Campaign** (single-player progression)
- **Modding Support** (custom unit/building definitions)
- **Anti-Cheat** (server audit log replication, client input validation)

---

## References

- **StarStrike.md:** Full architecture, gameplay systems, networking model, voxel design, rendering pipeline, deployment, risks
- **ARCHITECTURE_RECOMMENDATIONS.md:** Build system, PCH strategy, library architecture, dependency management
- **CODE_STANDARDS.md:** C++23 naming conventions, memory management, error handling (referenced but not included here)
- **MathConv.md:** DirectXMath migration plan (future; use fixed-point 12.12 for now, known working)
- **🆕 IMPLEMENTATION_VALIDATION.md:** Validation review (March 5, 2026) — all recommendations incorporated into this plan

---

**End of Implementation Plan**
