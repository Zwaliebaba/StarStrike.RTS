# Agent Guidance for StarStrike.RTS

## Mission
- Build, maintain, and polish the DirectX 12 game built on the Neuron engine.
- Follow existing architecture patterns and modern C++ conventions when touching the C++ code.
- Implement requested features or fixes within the workspace, respecting the provided instructions.

## Project Breakdown
- `NeuronCore/`: reusable utilities (math, file system, timers, debugging).
- `NeuronClient/`: low-level graphics/audio/input layers, including `GameMain`.
- `StarStrike/`: the game itself, which derives from `GameMain` and hosts gameplay logic.

## Key Patterns & Expectations
- Prefer inline class helpers over traditional singleton instances (static class members with `inline static`).
- Keep all engine code under the `Neuron::` namespaces per subdomain (Graphics, Audio, Timer, Client).
- Follow the naming convention: `_` prefix for parameters, `m_` for instance fields, `sm_` for statics, PascalCase for types/methods, camelCase for locals.
- Use modern C++20 features (`std::format`, `std::ranges`, `[[nodiscard]]`) and DirectXMath types for vector/matrix math.
- Prefer `winrt::com_ptr` for COM objects.
- Avoid adopting or mimicking legacy C-style code when adding new code (even if some legacy files like `elite.h` use it).

## Build & Tooling Notes
- The solution file is `StarStrike.slnx`. Only x64 builds are supported.
- Package management is handled through NuGet (`WindowsAppSDK`, `CppWinRT`, `PIX`); vcpkg is disabled (`VcpkgEnableManifest=false`).
- Run `clang-format` before landing changes (a `.clang-format` file lives in the repo root).
- Keep constant buffers 256-byte aligned and follow shader workflow (`.hlsl` in `StarStrike/Shaders/`, compiled to `CompiledShaders/`).
- When adding a new `.hlsl` shader, manually include it in the appropriate project file (e.g., `StarStrike.vcxproj`) under an `FxCompile` item and ensure the `ShaderModel` matches 6.7; the agent should document the location and any required metadata.

## Runtime & Integration Pointers
- Assets live under `{exe_path}/Assets/`, discovered via `FileSys::GetHomeDirectory()`.
- Graphics commands flow: get the command list, transition resources via the GPU resource state tracker, and allocate descriptors through `Graphics::Core` helpers.
- When building PSOs, supply root signatures, shader bytecode, input layouts (from `VertexTypes.h`), and finalize before use.
- Use `StarStrike::DX12Renderer` for 2D rendering primitives and text.
- Music and sound effects have separate engines (`MusicTrack`, `SoundEffect`) that load resources via `Audio::Core` helpers and expose looped or one-shot playback.

## Development Reminders
- Always end `DebugTrace` strings with `\n`.
- Prefer `ASSERT`/`DEBUG_ASSERT` macros for validation (with `DEBUG_ASSERT_TEXT` for detailed debug-only messages).
- There is no automated test suite; rely on manual verification or visual inspection.
- Respect the instructions in `.github/copilot-instructions.md` and refer back when in doubt.

## Agent Workflow
- Review the user request carefully, gather required context via `get_file`, `code_search`, `get_projects_in_solution`, and `get_files_in_project` as needed.
- Make minimal targeted changes, using `apply_patch` when editing files and validating with workspace tooling (build/test) when possible.
- Keep responses short and impersonal, and focus only on the user’s stated goals.
