# ── NeuronCore (header-only foundation: types, constants, platform stubs) ────
add_library(NeuronCore INTERFACE)

target_include_directories(NeuronCore
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:lib>
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

# Platform-specific link dependencies (applied when consumers link NeuronCore)
if(WIN32)
    target_link_libraries(NeuronCore INTERFACE ws2_32)
endif()
