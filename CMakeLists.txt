# StarStrike.RTS — Root CMake Configuration
cmake_minimum_required(VERSION 3.24)

project(StarStrike
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Voxel-based isometric RTS MMO"
)

# ── C++ Standard ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Platform Detection ───────────────────────────────────────────────────────
set(STARSTRIKE_WINDOWS ${WIN32})
set(STARSTRIKE_UNIX ${UNIX})
option(BuildServerOnly "Build server only (Linux headless)" OFF)

# ── Version Info ─────────────────────────────────────────────────────────────
set(STARSTRIKE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(STARSTRIKE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(STARSTRIKE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

# ── Server Configuration ────────────────────────────────────────────────────
set(SERVER_TICK_RATE_HZ 60)
set(SERVER_MAX_PLAYERS 50)
set(SERVER_PORT 7777)

# ── Voxel System ─────────────────────────────────────────────────────────────
set(VOXEL_CHUNK_SIZE 32)
set(SECTOR_GRID_X 4)
set(SECTOR_GRID_Y 4)

# ── Rendering ────────────────────────────────────────────────────────────────
set(CLIENT_TARGET_FPS 60)
set(RENDER_RESOLUTION_WIDTH 640)
set(RENDER_RESOLUTION_HEIGHT 360)

# ── Feature Flags ────────────────────────────────────────────────────────────
option(ENABLE_PROFILING "Enable performance profiling" ON)
option(ENABLE_VALIDATION_LAYERS "Enable DX12 validation layers" ON)
option(ENABLE_DEBUG_UI "Enable in-game debug UI" OFF)

# ── Generate config.h ───────────────────────────────────────────────────────
configure_file(
    lib/NeuronCore/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/config.h
    @ONLY
)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)

# ── Status ───────────────────────────────────────────────────────────────────
message(STATUS "StarStrike v${PROJECT_VERSION}")
message(STATUS "  Platform:   ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler:   ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Server Only: ${BuildServerOnly}")

# ── Subdirectories ───────────────────────────────────────────────────────────
add_subdirectory(lib/NeuronCore)
add_subdirectory(lib/GameLogic)
add_subdirectory(lib/NeuronServer)
add_subdirectory(lib/NeuronClient)

# Phase 1 Verification Target
add_executable(VerifyPhase1 tests/VerifyPhase1.cpp)
target_link_libraries(VerifyPhase1 PRIVATE NeuronCore)
target_include_directories(VerifyPhase1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/NeuronCore)

# Future phases:
# add_subdirectory(Server)
# if(NOT BuildServerOnly)
#     add_subdirectory(StarStrike)
# endif()
# if(BUILD_TESTING)
#     enable_testing()
#     add_subdirectory(tests)
# endif()
