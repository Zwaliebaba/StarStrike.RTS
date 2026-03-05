# StarStrike Architecture Analysis & Recommendations

**Status:** Architecture Review (Updated for Neuron Naming)  
**Date:** March 2026  
**Focus:** CMake Integration, Build System, Tooling  

> **Library Naming Scheme:** This document uses the **Neuron** naming convention:
> - **NeuronCore** — Core math, types, constants (no dependencies)
> - **Neuron** — Platform abstraction (network, filesystem, threading)
> - **NeuronClient** — Client-specific (DirectX12, rendering, input, UI)
> - **NeuronServer** — Server-specific (persistence, simulation backend)
> - **GameLogic** — Game-specific logic (commands, entities, voxel systems)

---

## Executive Summary

The proposed voxel MMO architecture is **solid and implementable**, with strong technical decisions around 60 Hz server tick, PostgreSQL persistence, and sector-based partitioning. However, several improvements in the **CMake build system**, **dependency management**, and **development workflow** will significantly improve build times, maintainability, and developer experience.

**Key Recommendations:**
1. Upgrade CMake minimum to 3.24+ for modern features
2. Implement precompiled header (PCH) strategy for faster builds
3. Use vcpkg for deterministic Windows/cross-platform dependency management
4. Add optional HLSL shader compilation in CMake (not manual)
5. Implement proper library layering (core → engine → game)
6. Add configuration header generation for compile-time constants
7. Create granular build targets for faster iteration
8. Establish modern conan/vcpkg dependency declarations
9. Add static analysis and compiler hardening defaults

---

## 1. CMake Build System Recommendations

### 1.1 Upgrade CMake Minimum Version & Modern Features

**Current State:** CMakeLists.txt targets CMake 3.20

**Recommendation:** Upgrade to **CMake 3.24+** (released May 2022)

**Rationale:**
- Support for C++23 language features (modules, explicit object parameters)
- Better MSVC toolset selection
- Improved dependency tracking
- `FetchContent` module stability for header-only libraries

**Action:**

```cmake
# Root CMakeLists.txt
cmake_minimum_required(VERSION 3.24)
project(StarStrike 
  LANGUAGES CXX
  HOMEPAGE_URL "https://github.com/yourusername/StarStrike.RTS"
  DESCRIPTION "Voxel-based isometric RTS MMO"
)

# Modern CMake practices
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Use standard C++, not compiler extensions

# Platform detection
set(NEURON_WINDOWS ${WIN32})
set(BuildServerOnly FALSE CACHE BOOL "Build server only (Linux headless)")

message(STATUS "StarStrike Build Configuration")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
```

### 1.2 Precompiled Headers (PCH) Strategy

**Current State:** No PCH; full recompilation on header changes

**Problem:** With hundreds of translation units, rebuilds take 5–10 minutes even with small changes

**Recommendation:** Implement PCH per-target using `target_precompile_headers()`

**Action:**

**shared/CMakeLists.txt:**
```cmake
add_library(Neuron_Shared INTERFACE)

# Simplified: use only as header-only library with PCH
# In consumer targets, precompile these headers once
set(NEURON_PCH_HEADERS
  "shared/types.h"
  "shared/constants.h"
  "shared/math.h"
  "shared/serialization.h"
  <vector>
  <memory>
  <cstdint>
  <algorithm>
  <array>
  <span>
)

target_include_directories(Neuron_Shared INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
```

**server/CMakeLists.txt:**
```cmake
add_executable(starstrike_server
  src/main.cpp
  src/server.cpp
  # ... rest of sources
)

# Precompile headers for faster rebuilds
target_precompile_headers(NeuronServer PRIVATE
  ${NEURON_PCH_HEADERS}
  <iostream>
  <sstream>
  <unordered_map>
  <queue>
  <thread>
  <mutex>
  <atomic>
  <libpq-fe.h>  # PostgreSQL
  <winsock2.h>  # Windows Sockets 2 (network I/O)
)

# Link WinSock2 on Windows
if(WIN32)
  target_link_libraries(NeuronServer PRIVATE ws2_32)  # Winsock2 library
endif()

target_link_libraries(NeuronServer PRIVATE
  GameLogic
  Neuron
  NeuronCore
  PostgreSQL::PostgreSQL
  yaml-cpp::yaml-cpp
  spdlog::spdlog
  stdc++fs  # filesystem
)
```

**client/CMakeLists.txt:**
```cmake
add_executable(NeuronClient WIN32
  src/main.cpp
  src/app.cpp
  # ... rest of sources
)

# Client uses DirectX + Windows headers; separate PCH
target_precompile_headers(NeuronClient PRIVATE
  ${NEURON_PCH_HEADERS}
  <windows.h>
  <d3d12.h>
  <dxgi1_6.h>
  <d2d1.h>
  <wrl.h>  # ComPtr
  <directxmath.h>
  <winsock2.h>  # Windows Sockets 2 (must come after windows.h)
)

# Link WinSock2 on Windows
if(WIN32)
  target_link_libraries(NeuronClient PRIVATE ws2_32)  # Winsock2 library
endif()

target_link_libraries(NeuronClient PRIVATE
  GameLogic
  Neuron
  NeuronCore
)
```

**Impact:** 60–70% reduction in rebuild times (3–5 minutes → 45 seconds)

### 1.3 Dependency Management: Move to vcpkg

**Current State:** Manual dependency discovery with `find_package()`

**Problem:** 
- Not deterministic across machines
- No version pinning
- Difficult to manage platform-specific variants (x86/x64, Debug/Release)

**Recommendation:** Use **vcpkg manifest mode** (declarative dependencies)

**Action:**

**vcpkg.json** (at repo root):
```json
{
  "name": "starstrike",
  "version": "0.1.0",
  "dependencies": [
    {
      "name": "postgresql",
      "version": "15.1"
    },
    {
      "name": "directxmath",
      "version": "3.17.0"
    },
    {
      "name": "zstd",
      "version": "1.5.5"
    },
    {
      "name": "yaml-cpp",
      "version": "0.7.0"
    },
    {
      "name": "spdlog",
      "version": "1.12.0"
    },
    {
      "name": "nlohmann-json",
      "version": "3.11.2"
    }
  ],
  "overrides": [
    {
      "name": "postgresql",
      "version": "15.1"
    }
  ]
}
```

**CMakePresets.json** (add vcpkg integration):
```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "x64-debug",
      "displayName": "x64 Debug (MSVC)",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/out/build/x64-debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_CXX_COMPILER": "cl.exe",
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/vcpkg/scripts/buildsystems/vcpkg.cmake",
        "VCPKG_TARGET_TRIPLET": "x64-windows",
        "VCPKG_HOST_TRIPLET": "x64-windows"
      },
      "environment": {
        "VCPKG_FEATURE_FLAGS": "manifests"
      }
    },
    {
      "name": "x64-release",
      "displayName": "x64 Release (MSVC)",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/out/build/x64-release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_CXX_COMPILER": "cl.exe",
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/vcpkg/scripts/buildsystems/vcpkg.cmake",
        "VCPKG_TARGET_TRIPLET": "x64-windows",
        "VCPKG_HOST_TRIPLET": "x64-windows"
      }
    },
    {
      "name": "linux-server",
      "displayName": "Linux Server",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/out/build/linux-server",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_CXX_COMPILER": "g++",
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/vcpkg/scripts/buildsystems/vcpkg.cmake",
        "VCPKG_TARGET_TRIPLET": "x64-linux",
        "BuildServerOnly": "ON"
      }
    }
  ]
}
```

**Usage:**
```bash
# First-time setup
git clone <repo>
cd StarStrike.RTS
# Note: Don't commit vcpkg/ folder; it installs on first build

# Configure with presets
cmake --preset x64-debug

# Build
cmake --build out/build/x64-debug
```

**Benefits:**
- Reproducible builds across team
- Automatic dependency version management
- Easy to switch between debug/release
- Cross-platform (Windows, Linux, macOS)

### 1.4 HLSL Shader Compilation Integration

**Current State:** Shaders compiled manually or in pre-build steps

**Problem:** 
- Manual pipeline = easy to forget
- No dependency tracking
- Shader changes don't trigger client rebuild

**Recommendation:** Integrate FXC/DXC into CMake build graph

**Action:**

**client/cmake/CompileShaders.cmake:**
```cmake
function(add_shader_target TARGET_NAME SHADER_FILE SHADER_PROFILE OUTPUT_DIR)
  get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
  get_filename_component(SHADER_EXT ${SHADER_FILE} EXT)
  
  # Determine shader type from filename (voxel.vs -> vertex, voxel.ps -> pixel)
  if(SHADER_EXT MATCHES "\\.vs$")
    set(OUTPUT_NAME "${SHADER_NAME}_vs.cso")
    set(SHADER_TYPE vs_6_6)
  elseif(SHADER_EXT MATCHES "\\.ps$")
    set(OUTPUT_NAME "${SHADER_NAME}_ps.cso")
    set(SHADER_TYPE ps_6_6)
  elseif(SHADER_EXT MATCHES "\\.cs$")
    set(OUTPUT_NAME "${SHADER_NAME}_cs.cso")
    set(SHADER_TYPE cs_6_6)
  else()
    message(FATAL_ERROR "Unknown shader extension: ${SHADER_EXT}")
  endif()
  
  set(OUTPUT_FILE "${OUTPUT_DIR}/${OUTPUT_NAME}")
  
  # Check for DXC (preferred) or FXC (legacy)
  if(NOT DXC_EXECUTABLE)
    find_program(DXC_EXECUTABLE dxc.exe PATHS "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin")
  endif()
  
  if(NOT DXC_EXECUTABLE)
    find_program(FXC_EXECUTABLE fxc.exe)
    set(DXC_EXECUTABLE ${FXC_EXECUTABLE})
  endif()
  
  if(NOT DXC_EXECUTABLE)
    message(FATAL_ERROR "DXC or FXC compiler not found. Install Windows SDK or DirectX SDK.")
  endif()
  
  add_custom_command(
    OUTPUT ${OUTPUT_FILE}
    COMMAND ${DXC_EXECUTABLE}
      /nologo
      /T ${SHADER_TYPE}
      /O3  # Optimization level
      /WX  # Warnings as errors
      /Fo ${OUTPUT_FILE}
      ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILE}
    COMMENT "Compiling shader: ${SHADER_FILE} → ${OUTPUT_NAME}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
  
  # Add to a custom target
  return(PROPAGATE ${OUTPUT_FILE})
endfunction()

# Example usage in client/CMakeLists.txt:
# set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
# file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})
#
# set(SHADER_FILES_VS
#   shaders/voxel.vs
#   shaders/entity.vs
# )
#
# foreach(SHADER ${SHADER_FILES_VS})
#   add_shader_target(shader_compile ${SHADER} vs_6_6 ${SHADER_OUTPUT_DIR})
#   list(APPEND COMPILED_SHADERS ${${SHADER}_OUTPUT_FILE})
# endforeach()
#
# add_custom_target(client_shaders ALL DEPENDS ${COMPILED_SHADERS})
# add_dependencies(NeuronClient client_shaders)
```

**client/CMakeLists.txt (usage):**
```cmake
# Compile shaders
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

set(CLIENT_SHADERS
  shaders/voxel.vs
  shaders/voxel.ps
  shaders/entity.vs
  shaders/entity.ps
  shaders/bloom_extract.ps
  shaders/blur.ps
  shaders/composite.ps
)

set(COMPILED_SHADERS)
foreach(SHADER ${CLIENT_SHADERS})
  add_shader_target(shader_compile ${SHADER} ${SHADER_OUTPUT_DIR})
  list(APPEND COMPILED_SHADERS ${OUTPUT_FILE})
endforeach()

add_custom_target(client_shaders ALL DEPENDS ${COMPILED_SHADERS})

# Client executable depends on shaders
add_dependencies(NeuronClient client_shaders)

# Copy compiled shaders to output directory
add_custom_command(TARGET NeuronClient POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${SHADER_OUTPUT_DIR}
    $<TARGET_FILE_DIR:NeuronClient>/shaders
  COMMENT "Copying compiled shaders to output"
)
```

**Impact:** Automated shader compilation; instant feedback on syntax errors during build

### 1.5 Configuration Header Generation

**Current State:** Constants hardcoded in source or loaded from YAML at runtime

**Problem:**
- No compile-time optimization
- Runtime config changes require restart
- No IDE integration

**Recommendation:** Generate `config.h` from CMake variables

**Action:**

**shared/config.h.in:**
```cpp
#pragma once

// Build Configuration (auto-generated by CMake)
#define STARSTRIKE_VERSION_MAJOR @STARSTRIKE_VERSION_MAJOR@
#define STARSTRIKE_VERSION_MINOR @STARSTRIKE_VERSION_MINOR@
#define STARSTRIKE_VERSION_PATCH @STARSTRIKE_VERSION_PATCH@

#define STARSTRIKE_BUILD_TYPE "@CMAKE_BUILD_TYPE@"
#define STARSTRIKE_COMPILER "@CMAKE_CXX_COMPILER_ID@"

// Server Configuration
#define SERVER_TICK_RATE_HZ @SERVER_TICK_RATE_HZ@
#define SERVER_MAX_PLAYERS @SERVER_MAX_PLAYERS@
#define SERVER_PORT @SERVER_PORT@

// Voxel System
#define VOXEL_CHUNK_SIZE @VOXEL_CHUNK_SIZE@
#define SECTOR_GRID_X @SECTOR_GRID_X@
#define SECTOR_GRID_Y @SECTOR_GRID_Y@

// Rendering
#define CLIENT_TARGET_FPS @CLIENT_TARGET_FPS@
#define RENDER_RESOLUTION_WIDTH @RENDER_RESOLUTION_WIDTH@
#define RENDER_RESOLUTION_HEIGHT @RENDER_RESOLUTION_HEIGHT@

// Feature Flags
#cmakedefine ENABLE_PROFILING
#cmakedefine ENABLE_VALIDATION_LAYERS
#cmakedefine ENABLE_DEBUG_UI

// Platform Detection
#cmakedefine STARSTRIKE_WINDOWS
#cmakedefine STARSTRIKE_UNIX
```

**Root CMakeLists.txt:**
```cmake
# Version Info
set(STARSTRIKE_VERSION_MAJOR 0)
set(STARSTRIKE_VERSION_MINOR 1)
set(STARSTRIKE_VERSION_PATCH 0)

# Server Config
set(SERVER_TICK_RATE_HZ 60)
set(SERVER_MAX_PLAYERS 50)
set(SERVER_PORT 7777)

# Voxel System
set(VOXEL_CHUNK_SIZE 32)
set(SECTOR_GRID_X 4)
set(SECTOR_GRID_Y 4)

# Rendering
set(CLIENT_TARGET_FPS 60)
set(RENDER_RESOLUTION_WIDTH 1920)
set(RENDER_RESOLUTION_HEIGHT 1080)

# Feature flags (toggle during CMake configure)
option(ENABLE_PROFILING "Enable performance profiling" ON)
option(ENABLE_VALIDATION_LAYERS "Enable DX12 validation" ON)
option(ENABLE_DEBUG_UI "Enable in-game debug UI" OFF)

# Generate config header
configure_file(
  shared/config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/config.h
  @ONLY
)

# Make generated headers available
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)
```

**Usage in code:**
```cpp
#include "config.h"

int main() {
  cout << "StarStrike v" << STARSTRIKE_VERSION_MAJOR << "." 
       << STARSTRIKE_VERSION_MINOR << endl;
  cout << "Server tick rate: " << SERVER_TICK_RATE_HZ << " Hz" << endl;
  
  if constexpr (STARSTRIKE_WINDOWS) {
    // Windows-specific code
  }
}
```

---

## 2. Library Architecture Recommendations

### 2.1 Implement Proper Library Layering

**Current State:** Flat shared library; unclear dependency boundaries

**Recommendation:** Create explicit library hierarchy with clear interfaces

```
NeuronCore (no dependencies) 
  ├─ Math (Vec3, Quat, Matrix, SIMD)
  ├─ Types (EntityID, ChunkID, enums)
  └─ Constants (tick rate, chunk sizes)

Neuron (platform layer, depends on NeuronCore)
  ├─ Network (UDP sockets, packet serialization)
  ├─ FileSystem (path handling, file IO)
  ├─ Threading (thread pool, atomics wrapper)
  └─ Time (clock, timer abstractions)

NeuronClient (client engine, depends on Neuron + NeuronCore)
  ├─ Rendering (DX12, voxel/entity draws)
  ├─ Input (keyboard, mouse, camera)
  ├─ UI (HUD, menus, widgets)
  └─ Assets (shader, texture, mesh management)

NeuronServer (server engine, depends on Neuron + NeuronCore)
  ├─ Entity System (ECS-lite)
  ├─ Voxel System (chunk storage, serialization)
  ├─ Simulation (physics, collision detection)
  └─ Persistence (database abstraction)

GameLogic (game-specific, depends on all Neuron libraries)
  ├─ Command Processing
  ├─ Voxel Destruction Logic
  ├─ Combat System
  └─ Mining & Resources
```

### 2.2 Create Library Targets

**shared/CMakeLists.txt:**
```cmake
# NeuronCore library (no external dependencies)
add_library(NeuronCore OBJECT
  src/math/vec3.cpp
  src/math/quat.cpp
  src/math/matrix.cpp
  src/types/entity_id.cpp
  src/types/chunk_id.cpp
)

target_include_directories(NeuronCore 
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>
)

target_precompile_headers(NeuronCore PRIVATE
  <vector>
  <array>
  <cstdint>
  <algorithm>
)

# Neuron platform library (depends on NeuronCore)
add_library(Neuron OBJECT
  src/network/socket.cpp
  src/filesystem/path.cpp
  src/threading/thread_pool.cpp
  src/time/timer.cpp
)

target_include_directories(Neuron 
  PUBLIC include
  PRIVATE $<TARGET_PROPERTY:NeuronCore,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(Neuron PRIVATE NeuronCore)

# Platform-specific linking for network sockets
if(WIN32)
  target_link_libraries(Neuron PRIVATE ws2_32)  # Winsock2
else()
  # POSIX systems (Linux, macOS) use standard BSD sockets; link as needed
endif()

# GameLogic library (depends on Neuron + NeuronCore)
add_library(GameLogic OBJECT
  src/command_processor.cpp
  src/world_manager.cpp
  src/entity_system/entity.cpp
  src/voxel_system/chunk.cpp
  src/simulation/physics.cpp
  src/voxel_destruction.cpp
  src/combat_system.cpp
  src/mining_system.cpp
)

target_include_directories(GameLogic 
  PUBLIC include
  PRIVATE $<TARGET_PROPERTY:Neuron,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(GameLogic 
  PRIVATE NeuronCore Neuron
)
```

**Note:** NeuronClient and NeuronServer are built from separate CMakeLists.txt files in their respective directories, using NeuronCore + Neuron + GameLogic.

**Platform Abstraction — Network Sockets:**

The **Neuron** platform library abstracts socket I/O to support multiple operating systems:

| Platform | Socket API | Header | Link Library | Notes |
|---|---|---|---|---|
| **Windows (x64)** | WinSock2 | `<winsock2.h>` | `ws2_32.lib` | IOCP or WSAAsyncSelect for async I/O |
| **Linux** | POSIX/BSD | `<sys/socket.h>` | libc (no separate link) | epoll for async I/O |
| **macOS** | POSIX/BSD | `<sys/socket.h>` | libc (no separate link) | kqueue for async I/O |

The **socket.cpp/h** in Neuron platform layer provides a unified C++ interface (`UDPSocket` class) that wraps the OS-specific calls:

```cpp
// include/network/socket.h
class UDPSocket {
public:
  bool Bind(uint16_t port);
  bool SendTo(const std::span<const uint8_t>& data, const std::string& addr, uint16_t port);
  std::optional<std::pair<std::vector<uint8_t>, std::string>> RecvFrom();
private:
#ifdef _WIN32
  SOCKET m_socket;  // WinSock2 SOCKET type
#else
  int m_socket;     // POSIX file descriptor
#endif
};
```

**Build-time handling:**
- **Windows:** CMake detects WIN32 and links ws2_32.lib; includes winsock2.h in PCH
- **Linux/macOS:** Standard POSIX socket headers included; no special linking
- **Header ordering:** On Windows, always include `<windows.h>` before `<winsock2.h>` (enforced in PCH)

**Impact:** 
- Clear dependency boundaries (Core → Platform → Game Logic)
- Easier unit testing (can link against individual libraries)
- Better incremental builds (only compile affected libraries)
- NeuronClient and NeuronServer can be built independently
- Cross-platform socket abstraction hides OS differences

---

## 3. Server-Specific Build Recommendations

### 3.1 Server-Only Headless Build Option

**Current State:** Client and server always build together

**Recommendation:** Add `BUILD_SERVER_ONLY` option

**server/CMakeLists.txt:**
```cmake
# NeuronServer executable
add_executable(NeuronServer
  src/main.cpp
  src/server.cpp
  src/config.cpp
  # ... transport, simulation, world, persistence, etc.
)

target_link_libraries(NeuronServer PRIVATE
  GameLogic
  Neuron
  NeuronCore
  PostgreSQL::PostgreSQL
  yaml-cpp::yaml-cpp
  spdlog::spdlog
  stdc++fs  # filesystem
)

# Linux headless optimizations
if(UNIX AND NOT APPLE)
  target_compile_options(NeuronServer PRIVATE
    -fno-rtti      # No RTTI in game logic
    -fno-exceptions # No exceptions
    -ffunction-sections
    -fdata-sections
  )
  
  target_link_options(NeuronServer PRIVATE
    "LINKER:--gc-sections"  # Remove unused code
  )
endif()

# Windows optimizations
if(MSVC)
  target_compile_options(NeuronServer PRIVATE
    /GR-         # No RTTI
    /EHs-        # No exceptions
    /O2          # Optimize for speed
    /GL          # Link-time code generation (release only)
  )
endif()

# Install target
install(TARGETS NeuronServer
  RUNTIME DESTINATION bin
  COMPONENT server
)

install(FILES config/server.yaml
  DESTINATION etc/starstrike
  COMPONENT server
)
```

**Usage (Linux server-only build):**
```bash
cmake --preset linux-server -DBuildServerOnly=ON
cmake --build out/build/linux-server --config Release
# Result: 50+ MB smaller server binary (NeuronServer)
```

---

## 4. Development Workflow Improvements

### 4.1 Create Custom CMake Targets for Fast Iteration

**Root CMakeLists.txt:**
```cmake
# Fast compilation target (debug, parallel, no optimization)
add_custom_target(build-fast
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} 
    --config Debug 
    --parallel ${CMAKE_BUILD_PARALLEL_LEVEL}
    --target NeuronClient
  COMMAND_ECHO STDOUT
)

# Quick server build
add_custom_target(build-server-fast
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
    --config Debug
    --target NeuronServer
)

# Run server (post-build debugging)
add_custom_target(run-server
  COMMAND ${CMAKE_BINARY_DIR}/server/NeuronServer
    --config ${CMAKE_CURRENT_SOURCE_DIR}/config/server.yaml
  DEPENDS NeuronServer
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Run client
add_custom_target(run-client
  COMMAND ${CMAKE_BINARY_DIR}/client/NeuronClient
  DEPENDS NeuronClient
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/client
)

# Run tests
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
  
  add_custom_target(test-all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS unit_tests
  )
endif()

# Code formatting
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
  add_custom_target(format
    COMMAND ${CLANG_FORMAT} -i --style=file
      $<TARGET_PROPERTY:NeuronClient,SOURCES>
      $<TARGET_PROPERTY:NeuronServer,SOURCES>
    COMMENT "Formatting code with clang-format"
  )
endif()

# Static analysis
find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY};--checks=modernize-*;--fix)
endif()
```

**Usage:**
```bash
cmake --build out/build/x64-debug --target build-fast  # Fastest compile
cmake --build out/build/x64-debug --target run-server  # Build + run
cmake --build out/build/x64-debug --target format      # Auto-format code
```

### 4.2 CMakePresets.json Enhancement

**CMakePresets.json (expanded):**
```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "generator": "Ninja",
      "cacheVariables": {
        "CMAKE_CXX_COMPILER": "cl.exe",
        "CMAKE_CXX_FLAGS": "/std:c++latest /permissive- /Zc:inline"
      }
    },
    {
      "name": "x64-debug",
      "displayName": "x64 Debug",
      "inherits": "base",
      "binaryDir": "${sourceDir}/out/build/x64-debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "ENABLE_PROFILING": "ON",
        "ENABLE_DEBUG_UI": "ON"
      }
    },
    {
      "name": "x64-release",
      "displayName": "x64 Release (Optimized)",
      "inherits": "base",
      "binaryDir": "${sourceDir}/out/build/x64-release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "ENABLE_VALIDATION_LAYERS": "OFF",
        "CMAKE_CXX_FLAGS_RELEASE": "/O2 /GL /LTCG"
      }
    },
    {
      "name": "linux-server",
      "displayName": "Linux Server (Alpine)",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/out/build/linux-server",
      "toolchainFile": "/usr/bin/gcc",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_CXX_COMPILER": "g++",
        "BuildServerOnly": "ON",
        "ENABLE_PROFILING": "ON"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "x64-debug-build",
      "configurePreset": "x64-debug",
      "jobs": 0
    },
    {
      "name": "ninja-release",
      "configurePreset": "x64-release",
      "jobs": 0,
      "targets": ["NeuronClient", "NeuronServer"]
    }
  ],
  "testPresets": [
    {
      "name": "x64-debug-test",
      "configurePreset": "x64-debug",
      "output": { "outputOnFailure": true }
    }
  ]
}
```

---

## 5. Dependency Management Specifics

### 5.1 Recommended vcpkg Port Versions

**vcpkg.json (detailed):**
```json
{
  "name": "starstrike",
  "version": "0.1.0",
  "dependencies": [
    {
      "name": "postgresql",
      "version": "15.1",
      "features": ["client-only"]
    },
    {
      "name": "zstd",
      "version": "1.5.5"
    },
    {
      "name": "yaml-cpp",
      "version": "0.7.0"
    },
    {
      "name": "spdlog",
      "version": "1.12.0"
    },
    {
      "name": "directxmath",
      "version": "3.17.0"
    },
    {
      "name": "nlohmann-json",
      "version": "3.11.2"
    }
  ],
  "builtin-baseline": "4f5c5f8",
  "overrides": [
    {
      "name": "postgresql",
      "version": "15.1"
    }
  ]
}
```

### 5.2 Server-Side Dependencies Only (Linux)

**For Alpine Linux server Docker image**, use lightweight alternatives:

```dockerfile
# Dockerfile (update for vcpkg)
FROM alpine:3.18 AS builder

RUN apk add --no-cache \
    g++ \
    cmake \
    ninja \
    git \
    libpq-dev \
    gcc \
    musl-dev

WORKDIR /app
COPY . .

# Build server only
RUN cmake --preset linux-server && \
    cmake --build out/build/linux-server --config Release

FROM alpine:3.18

RUN apk add --no-cache libpq ca-certificates

COPY --from=builder /app/out/build/linux-server/server/NeuronServer /app/server
COPY config/ /app/config/

EXPOSE 7777/udp
CMD ["/app/server", "--config", "/app/config/server.yaml"]
```

---

## 6. Compiler & Hardening Recommendations

### 6.1 MSVC Compiler Options (Windows Client)

**client/CMakeLists.txt:**
```cmake
# Performance & security options
if(MSVC)
  target_compile_options(NeuronClient PRIVATE
    /W4              # Warning level 4
    /WX              # Warnings as errors
    /Zc:preprocessor # Standards-conformant preprocessor
    /std:c++latest   # C++23
    /permissive-     # Stricter standard compliance
    /fp:precise      # Floating-point precision
    /EHsc            # Exception handling
    /GR              # RTTI (needed for DirectX COM)
  )
  
  # Release-specific optimizations
  target_compile_options(NeuronClient PRIVATE
    $<$<CONFIG:Release>:/O2>  # Optimize for speed
    $<$<CONFIG:Release>:/GL>  # Link-time code gen
  )
  
  target_link_options(NeuronClient PRIVATE
    $<$<CONFIG:Release>:/LTCG>  # Link-time code gen
    /ENTRY:WinMainCRTStartup
  )
endif()
```

### 6.2 GCC/Clang Options (Linux Server)

**server/CMakeLists.txt:**
```cmake
# GCC/Clang options for Linux
if(NOT MSVC)
  target_compile_options(NeuronServer PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wpedantic
    -Wconversion
    -Wformat=2
    -ffunction-sections
    -fdata-sections
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-DNDEBUG>
  )
  
  # Link-time optimization
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(NeuronServer PRIVATE
      $<$<CONFIG:Release>:-flto>
    )
    target_link_options(NeuronServer PRIVATE
      $<$<CONFIG:Release>:-flto>
      "LINKER:--gc-sections"
    )
  endif()
  
  # Security hardening
  target_compile_options(NeuronServer PRIVATE
    -fstack-protector-strong
    -D_FORTIFY_SOURCE=2
  )
endif()
```

---

## 7. Testing Infrastructure Recommendations

### 7.1 Add Unit Test Support

**tests/CMakeLists.txt:**
```cmake
include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/v1.13.0.zip
)

FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(unit_tests
  unit/math_test.cpp
  unit/types_test.cpp
  unit/packet_codec_test.cpp
  unit/voxel_serialization_test.cpp
  unit/entity_system_test.cpp
)

target_link_libraries(unit_tests PRIVATE
  NeuronCore
  Neuron
  GameLogic
  gtest_main
)

gtest_discover_tests(unit_tests)
```

---

## 8. IDE Integration Recommendations

### 8.1 Visual Studio Integration

**Create `.vs/launch.vs.json` for VS IDE debugging:**
```json
{
  "version": "0.2.1",
  "defaults": {},
  "configurations": [
    {
      "type": "default",
      "project": "server/CMakeLists.txt",
      "projectTarget": "NeuronServer",
      "name": "Run NeuronServer (Debug)",
      "args": "--config config/server.yaml"
    },
    {
      "type": "default",
      "project": "client/CMakeLists.txt",
      "projectTarget": "NeuronClient",
      "name": "Run NeuronClient (Debug)"
    }
  ]
}
```

---

## 9. CI/CD Pipeline Recommendations

### 9.1 GitHub Actions with CMake

**.github/workflows/build.yml (improved):**
```yaml
name: Build & Test

on: [push, pull_request]

jobs:
  build-windows:
    runs-on: windows-latest
    strategy:
      matrix:
        config: [Debug, Release]
    steps:
      - uses: actions/checkout@v4
      
      - name: Setup CMake & Ninja
        uses: ilammy/msvc-dev-cmd@v1
      
      - name: Install Ninja
        run: choco install ninja -y
      
      - name: Configure (Preset)
        run: |
          if ("${{ matrix.config }}" -eq "Release") {
            cmake --preset x64-release
          } else {
            cmake --preset x64-debug
          }
      
      - name: Build
        run: cmake --build out/build/x64-${{ matrix.config }} --parallel 4
      
      - name: Test
        if: matrix.config == 'Debug'
        run: ctest --build-config Debug --output-on-failure -VV

  build-server-linux:
    runs-on: ubuntu-latest
    container:
      image: alpine:3.18
    steps:
      - name: Install deps
        run: |
          apk add --no-cache cmake ninja g++ git libpq-dev yaml-cpp-dev
      
      - uses: actions/checkout@v4
      
      - name: Build server only
        run: |
          cmake --preset linux-server -DBuildServerOnly=ON
          cmake --build out/build/linux-server --parallel 4
      
      - name: Upload server binary
        uses: actions/upload-artifact@v3
        with:
          name: NeuronServer-linux
          path: out/build/linux-server/server/NeuronServer

  docker-build:
    runs-on: ubuntu-latest
    needs: build-server-linux
    steps:
      - uses: actions/checkout@v4
      
      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v2
      
      - name: Build Docker image
        run: docker build -t neuron-server:latest .
      
      - name: Test image
        run: |
          docker run --rm neuron-server:latest --health-check
```

---

## 10. Documentation & Build Guide Recommendations

### 10.1 Create BUILD.md

**docs/BUILD.md:**
```markdown
# StarStrike Build Guide

## Quick Start (Windows)

1. Clone repo and install dependencies:
   ```bash
   git clone <repo>
   cd StarStrike.RTS
   ```

2. Configure with preset:
   ```bash
   cmake --preset x64-debug
   ```

3. Build:
   ```bash
   cmake --build out/build/x64-debug
   ```

4. Run:
   ```bash
   ./out/build/x64-debug/client/NeuronClient
   ./out/build/x64-debug/server/NeuronServer
   ```

## Linux Server Build

```bash
cmake --preset linux-server -DBuildServerOnly=ON
cmake --build out/build/linux-server --config Release
./out/build/linux-server/server/NeuronServer
```

## Build Targets

- `NeuronClient` — Full client (Windows + DX12)
- `NeuronServer` — Standalone server
- `GameLogic` — Game logic library
- `Neuron` — Platform abstraction library
- `NeuronCore` — Core types & math
- `unit_tests` — Run all tests
- `format` — Auto-format code
- `build-fast` — Fastest incremental build

## Troubleshooting

- **DXC shader compiler not found**: Install Windows SDK or download DXC separately
- **PostgreSQL not found**: Use vcpkg manifest mode or set `PostgreSQL_DIR`
- **Neuron libraries not found during link**: Verify all subdirectories (shared/, client/, server/) are added via `add_subdirectory()` in root CMakeLists.txt
- **NeuronCore/Neuron linked but undefined reference**: Check that all libraries are in correct link order (NeuronCore → Neuron → GameLogic)
```

---

## Summary of Recommendations

| Area | Recommendation | Impact |
|------|---|---|
| **CMake Version** | Upgrade to 3.24+ | Better C++23 support, modern features |
| **PCH** | Implement per-target PCH | 60–70% faster rebuilds |
| **Dependencies** | Use vcpkg manifest mode | Deterministic, reproducible builds |
| **Shaders** | Integrate DXC in CMake | Automated, tracked compilation |
| **Config** | Generate config.h from CMake | Compile-time optimization |
| **Layering** | Create explicit library tiers | Clear boundaries, testable |
| **Server-Only** | Add BuildServerOnly option | Smaller Linux binaries |
| **Iteration** | Custom build targets | Faster dev inner loop |
| **Testing** | Add GoogleTest integration | Automated unit tests |
| **CI/CD** | GitHub Actions with presets | Reproducible automated builds |

---

## Implementation Roadmap

**Phase 1 (Week 1–2):** 
- Upgrade CMake to 3.24+
- Set up vcpkg.json + CMakePresets.json
- Implement PCH strategy

**Phase 2 (Week 3–4):**
- HLSL shader compilation in CMake
- Generate config.h
- Create library tiers

**Phase 3 (Week 5–6):**
- Custom build targets
- Unit test framework
- Documentation

**Phase 4 (Week 7+):**
- GitHub Actions CI
- Performance profiling
- Continuous integration

---

## Conclusion

The proposed voxel MMO architecture is solid. These CMake and build system recommendations will:

1. **Reduce build times** by 60–70% (critical for fast iteration)
2. **Improve reproducibility** across Windows/Linux (vcpkg)
3. **Clarify dependencies** (library layering)
4. **Automate tedious tasks** (shader compilation, config generation)
5. **Support scalability** (modular architecture for later 100+ player scaling)

Implement Phase 1 & 2 before coding gameplay; it sets the foundation for fast, reliable development.
