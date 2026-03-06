# ── NeuronCore (foundation: types, math, file system, event bus, timers) ──────
add_library(NeuronCore STATIC
    pch.cpp
    NeuronCore.cpp
    FileSys.cpp
    WndProcManager.cpp
)

target_include_directories(NeuronCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:lib>
)

# Precompiled header — every .cpp in this target includes pch.h first
target_precompile_headers(NeuronCore PRIVATE pch.h)

# Platform-specific link dependencies
if(WIN32)
    target_link_libraries(NeuronCore PUBLIC ws2_32 onecore)
endif()
