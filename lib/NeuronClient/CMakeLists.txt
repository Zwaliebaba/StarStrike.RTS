# ── NeuronClient (DX12 rendering, input, audio, UI) ─────────────────────────
add_library(NeuronClient STATIC
    pch.cpp
    NeuronClient.cpp
    DDSTextureLoader.cpp
    DescriptorHeap.cpp
    GpuBuffer.cpp
    GraphicsCommon.cpp
    GraphicsCore.cpp
    PipelineState.cpp
    ResourceStateTracker.cpp
    RootSignature.cpp
    SamplerManager.cpp
)

target_include_directories(NeuronClient
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# Precompiled header — every .cpp in this target includes pch.h first
target_precompile_headers(NeuronClient PRIVATE pch.h)

target_link_libraries(NeuronClient PUBLIC NeuronCore GameLogic d3d12.lib dxgi.lib dxguid.lib)

# PIX profiling (debug builds only, via vcpkg)
find_package(winpixevent CONFIG QUIET)
if(winpixevent_FOUND)
    target_link_libraries(NeuronClient PUBLIC Microsoft::WinPixEventRuntime)
endif()
