# Xbox 360 to Windows 11 Porting Plan

## Executive Summary

This document outlines the **minimal-change porting strategy** for making the **StarStrike.RTS** game engine run on Windows 11. The goal is to get the game working with the fewest code changes possible, retaining existing APIs (D3D9, DirectInput, Win32 threading) where they remain functional on Windows.

The codebase consists of 20 C++ projects forming a complete game engine with rendering, physics, audio, networking, and input subsystems.

---

## 1. Solution Architecture Analysis

### Current Project Structure (20 Projects)

| Project | Purpose | Porting Work Required |
|---------|---------|----------------------|
| `xgame` | Main game application | Low - remove `#ifdef XBOX` blocks |
| `xcore` | Core utilities, memory, threading | **High** - replace Xbox memory APIs |
| `xsystem` | System abstractions, file I/O | **High** - remove Xbox system calls |
| `xrender` | Direct3D 9 rendering | Medium - D3D9 works, remove Xbox extensions |
| `xgameRender` | Game-specific rendering | Low - depends on xrender |
| `xinputsystem` | Input handling (XInput/DirectInput) | Low - APIs compatible on Windows |
| `xsound` | WWise audio integration | Medium - need Windows WWise SDK |
| `xnetwork` | Networking (WinSock) | Low - WinSock is Windows-native |
| `xphysics` | Havok physics integration | Medium - need Windows Havok SDK |
| `xgranny` | Granny animation middleware | Medium - need Windows Granny SDK |
| `xvisual` | Visual effects | Low - no Xbox-specific code expected |
| `xparticles` | Particle systems | Low - no Xbox-specific code expected |
| `terrain` | Terrain rendering | Low - no Xbox-specific code expected |
| `xgeom` | Geometry processing | None |
| `ximage` | Image loading/processing | Low |
| `xscript` | Scripting system | None |
| `ddx` | DDX texture format | Low |
| `compression` | Data compression | None |
| `shaders` | HLSL shaders | Low - D3D9 SM3.0 works on Windows |
| `xvince` | Analytics/telemetry | None - can be disabled |

---

## 2. Required Changes for Windows Compatibility

### 2.1 Graphics API (Keep D3D9)

**Current State:** Direct3D 9 with Xbox 360 extensions

**Target State:** Direct3D 9 on Windows (no API migration needed)

> **Note:** D3D9 is fully supported on Windows 11. No migration to D3D11/D3D12 is required.

| Component | Xbox 360 | Windows Action |
|-----------|----------|----------------|
| Device creation | `IDirect3D9::CreateDevice` | **Keep as-is** - works on Windows |
| Texture loading | XPR format | Convert assets to DDS (one-time) |
| Shaders | Xbox 360 compiler | **Keep as-is** - D3D9 SM3.0 works |
| Vertex declarations | `D3DVERTEXELEMENT9` | **Keep as-is** - D3D9 API |
| Effects framework | D3DX Effects | **Keep as-is** - use legacy DirectX SDK |
| Render states | `SetRenderState` | **Keep as-is** - D3D9 API |

### 2.2 Memory Management (Required)

**Current State:** Custom allocators with Xbox 360 memory model
- `XMemAlloc`/`XMemFree` for Xbox memory
- Rockall heap allocator
- Physical vs. cached memory distinction

**Target State:** Standard Windows memory management

```cpp
// Before (Xbox 360)
#ifdef XBOX
   void* ptr = XMemAlloc(size, XALLOC_PHYSICAL);
#endif

// After (Windows 11)
void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
// Or use standard allocators
void* ptr = _aligned_malloc(size, alignment);
```

### 2.3 Input System (Already Compatible)

**Current State:** DirectInput 8 + XInput

**Target State:** Keep existing APIs (fully Windows-compatible)

| Feature | Windows Action |
|---------|----------------|
| Gamepad (XInput) | **Keep as-is** - XInput 1.4 is compatible |
| Keyboard/Mouse (DirectInput 8) | **Keep as-is** - still supported |
| Rumble (`XInputSetState`) | **Keep as-is** - compatible |

### 2.4 Audio System (SDK Swap)

- Obtain Windows WWise SDK from Audiokinetic
- API should be compatible; library swap only

### 2.5 Physics Engine (SDK Swap)

- Obtain Windows x64 Havok libraries from Microsoft
- API should be compatible; library swap only

### 2.6 Networking (Already Compatible)

- **Keep as-is** - WinSock is Windows-native
- Remove any Xbox LIVE-specific code

### 2.7 Threading Model (Already Compatible)

- **Keep as-is** - Win32 threading APIs are native Windows

---

## 3. Build System Updates

### 3.1 Project Files

**Required Changes:**
1. Create new platform configurations for x64 Windows
2. Remove Xbox 360 platform toolset references
3. Update to Visual Studio 2022 toolset (v143)

### 3.2 Dependencies Update

| Library | Windows Version | Notes |
|---------|-----------------|-------|
| WWise | WWise Windows SDK | Contact Audiokinetic |
| Havok | Havok Windows x64 SDK | Contact Microsoft |
| Granny | Granny Windows x64 SDK | Contact RAD Game Tools |
| Bink | Bink Windows SDK | Contact RAD Game Tools |
| D3DX | D3DX9 (legacy DirectX SDK) | **Keep as-is** |

---

## 4. Phased Porting Approach

### Phase 1: Build Setup (Week 1)
1. Set up Windows x64 build configurations
2. Obtain Windows middleware SDKs
3. Configure include/library paths

### Phase 2: Core Libraries (Weeks 2-3)
4. Port `xcore` - replace `XMemAlloc`/`XMemFree`, remove `#ifdef XBOX`
5. Port `xsystem` - remove Xbox-specific system calls

### Phase 3: Rendering & Middleware (Weeks 4-5)
6. Port `xrender` (keep D3D9) - remove Xbox 360 D3D extensions
7. Swap middleware libraries (Havok, WWise, Granny, Bink)

### Phase 4: Integration & Testing (Week 6)
8. Build full solution
9. Fix remaining compile errors
10. Runtime testing and bug fixes

**Estimated Total: 6 weeks**

---

## 5. Code Changes Required

### 5.1 Preprocessor Conditionals

**Current State:** Heavy use of `#ifdef XBOX` for Xbox-specific code

**Target State:** Minimal conditionals, mostly for audio/physics SDK swaps

> **Note:** Keep Xbox-specific code only where absolutely necessary (e.g., analytics)

**Examples:**
```cpp
// Before (Xbox 360)
#ifdef XBOX
   XMemFree(ptr);
#endif

// After (Windows 11)
free(ptr); // or _aligned_free(ptr);
```

### 5.2 API Replacements Summary

| Xbox 360 API | Windows Replacement |
|--------------|---------------------|
| `XMemAlloc` | `VirtualAlloc` / `_aligned_malloc` |
| `XMemFree` | `VirtualFree` / `_aligned_free` |
| `XGetVideoMode` | D3D9 `GetAdapterDisplayMode` |
| D3D9 Xbox extensions | Remove/stub |
| Xbox 360 shaders | **Keep as-is** |

### 5.3 Removed Features
- Xbox LIVE integration
- Title-managed storage
- Xbox Guide overlay

---

## 6. Testing Strategy

1. **Compile Tests** - All projects build without errors
2. **Startup Test** - Game launches and reaches main menu
3. **Gameplay Test** - Core game loop functions
4. **Input Test** - Keyboard, mouse, and gamepad work
5. **Audio Test** - Sound effects and music play

---

## 7. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Missing Windows middleware licenses | High | Critical | Contact vendors early |
| Xbox-specific code deeply embedded | Medium | Medium | Thorough code search |
| Asset format incompatibilities | Low | Medium | Convert XPR to DDS |

---

## 8. Recommended Tools

- **Visual Studio 2022** - IDE and compiler
- **PIX for Windows** or **RenderDoc** - Graphics debugging
- **DirectX SDK (June 2010)** - Legacy D3D9/D3DX headers
- **Windows SDK** - Windows APIs

---

## 9. Next Steps

1. Contact vendors for Windows middleware licenses
2. Create Windows x64 solution configuration
3. Begin with `xcore` and `xsystem`
4. Set up CI for Windows builds

---

## Appendix A: Files Requiring Changes

### Critical Files
- `xcore/memory/XMemory.h` - Replace Xbox memory APIs
- `xcore/memory/XMemory.cpp` - Replace Xbox memory APIs
- `xgame/xboxMemory.h` - Remove or stub
- `xrender/BD3D.h` - Remove Xbox D3D extensions
- `xrender/BD3D.cpp` - Remove Xbox D3D extensions

### Shader Files
- **No changes required** - D3D9 SM3.0 shaders work on Windows

---

## Appendix B: Future Modernization (Optional)

After the port is complete, consider these **optional** improvements as separate projects:

| Enhancement | Benefit | Effort |
|-------------|---------|--------|
| D3D11/D3D12 migration | Better performance | High |
| C++17/20 adoption | Modern language features | Medium |
| std::thread migration | Cross-platform threading | Low |
| HDR/VRR support | Modern display features | Medium |

---

## Appendix C: Agent Search Patterns

This section provides specific patterns for automated code refactoring tools.

### C.1 Xbox Memory APIs to Replace

| Search Pattern | Replace With |
|----------------|--------------|
| `XMemAlloc(size, XALLOC_PHYSICAL)` | `VirtualAlloc(nullptr, size, MEM_COMMIT \| MEM_RESERVE, PAGE_READWRITE)` |
| `XMemAlloc(size, flags)` | `_aligned_malloc(size, 16)` |
| `XMemFree(ptr, flags)` | `_aligned_free(ptr)` |
| `XPhysicalAlloc(` | `VirtualAlloc(` |
| `XPhysicalFree(` | `VirtualFree(` |

### C.2 Preprocessor Blocks to Process

**Remove entire block (keep nothing):**
```cpp
#ifdef XBOX
   // Xbox-only code - DELETE entire block
#endif
```

**Keep else branch only:**
```cpp
#ifdef XBOX
   // DELETE this branch
#else
   // KEEP this branch
#endif
```

**Search patterns:**
- `#ifdef XBOX`
- `#if defined(XBOX)`
- `#if defined(_XBOX)`
- `#ifndef XBOX` (inverse - keep the #ifdef content)

### C.3 Xbox LIVE APIs to Remove/Stub

| API | Action |
|-----|--------|
| `XNetStartup` | Remove call |
| `XNetCleanup` | Remove call |
| `XUserGetSigninState` | Return `eXUserSigninState_NotSignedIn` |
| `XNotifyCreateListener` | Return `INVALID_HANDLE_VALUE` |
| `XNotifyGetNext` | Return `FALSE` |
| `XLiveInitialize` | Remove call |
| `XLiveUninitialize` | Remove call |
| `XNetGetTitleXnAddr` | Remove/stub |

### C.4 Xbox D3D Extensions to Remove

| API | Action |
|-----|--------|
| `IDirect3DDevice9::SetPredication` | Remove call |
| `IDirect3DDevice9::BeginTiling` | Remove call |
| `IDirect3DDevice9::EndTiling` | Remove call |
| `IDirect3DDevice9::SetRingBufferParameters` | Remove call |
| `D3DDevice::InsertFence` | Remove call |
| `D3DDevice::BlockOnFence` | Remove call |

### C.5 File Search Patterns

**Files likely containing Xbox code:**
```
**/xboxMemory.*
**/XMemory.*
**/*Xbox*.*
**/*XBOX*.*
```

**Search in all files for:**
```
XMemAlloc
XMemFree
XPhysicalAlloc
XNetStartup
XUserGetSigninState
#ifdef XBOX
#if defined(XBOX)
```

### C.6 Project File Updates

**In `.vcxproj` files, remove/update:**
- `<Platform>Xbox 360</Platform>` ? `<Platform>x64</Platform>`
- `<PlatformToolset>Xbox360</PlatformToolset>` ? `<PlatformToolset>v143</PlatformToolset>`
- Remove `<XdkEdition>` elements
- Remove Xbox-specific `<AdditionalIncludeDirectories>` paths

### C.7 Verification Commands

After changes, verify with:
```powershell
# Find remaining Xbox references
Get-ChildItem -Recurse -Include *.cpp,*.h,*.c | Select-String -Pattern "XBOX|XMemAlloc|XPhysical" | Select-Object -First 20

# Count #ifdef XBOX blocks remaining
Get-ChildItem -Recurse -Include *.cpp,*.h | Select-String -Pattern "#ifdef XBOX" | Measure-Object
```

---

*Document updated for Copilot agent compatibility*
