# ── NeuronCore (header-only foundation: types, constants, math) ──────────────
add_library(NeuronCore INTERFACE)

target_include_directories(NeuronCore
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Propagate PCH headers to consuming targets
target_precompile_headers(NeuronCore
    INTERFACE
        <algorithm>
        <array>
        <atomic>
        <cmath>
        <cstddef>
        <cstdint>
        <cstring>
        <functional>
        <limits>
        <memory>
        <optional>
        <span>
        <string>
        <string_view>
        <unordered_map>
        <utility>
        <variant>
        <vector>
)

# ── Neuron (platform abstraction: network, filesystem, threading, time) ──────
# Starts as INTERFACE (stubs only); will become OBJECT when .cpp files are added.
add_library(Neuron INTERFACE)

target_include_directories(Neuron
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(Neuron INTERFACE NeuronCore)

# Platform-specific link dependencies (applied when consumers link Neuron)
if(WIN32)
    target_link_libraries(Neuron INTERFACE ws2_32)
endif()

# ── Phase 1 Verification Target ─────────────────────────────────────────────
add_executable(VerifyPhase1 tests/VerifyPhase1.cpp)
target_link_libraries(VerifyPhase1 PRIVATE Neuron)
target_include_directories(VerifyPhase1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
