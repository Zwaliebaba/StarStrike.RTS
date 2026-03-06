# ── StarStrike client executable ──────────────────────────────────────────────
add_executable(StarStrike WIN32
    pch.cpp
    WinMain.cpp
    GameApp.cpp
)

target_include_directories(StarStrike
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Precompiled header — every .cpp in this target includes pch.h first
target_precompile_headers(StarStrike PRIVATE pch.h)

target_link_libraries(StarStrike PRIVATE NeuronClient GameLogic NeuronCore)
