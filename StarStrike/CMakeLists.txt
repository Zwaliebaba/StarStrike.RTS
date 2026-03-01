# CMakeLists.txt : StarStrike executable
# Depends on NeuronClient (which transitively brings in NeuronCore).
#
# CONFIGURE_DEPENDS: CMake will re-glob and re-configure automatically
# whenever .cpp or .h files are added to or removed from this directory.

file(GLOB STARSTRIKE_SRC CONFIGURE_DEPENDS
    "*.cpp"
    "*.h"
)

add_executable (StarStrike WIN32 ${STARSTRIKE_SRC} Deliv.rc)

# Make lex-generated scanner symbols static (same as NeuronClient).
target_compile_definitions (StarStrike
    PRIVATE
        YY_STATIC
)

target_precompile_headers (StarStrike
    PRIVATE
        pch.h
)

# PRIVATE: StarStrike is the final executable; nothing consumes it downstream.
target_link_libraries (StarStrike
    PRIVATE
        NeuronClient
        # DirectX SDK libraries
        ${CMAKE_SOURCE_DIR}/DX9/Lib/d3d9.lib
        ${CMAKE_SOURCE_DIR}/DX9/Lib/dplayx.lib
        ${CMAKE_SOURCE_DIR}/DX9/Lib/dxguid.lib
        ${CMAKE_SOURCE_DIR}/DX9/Lib/dsound.lib
        ${CMAKE_SOURCE_DIR}/DX9/Lib/dinput.lib
        # Third-party libraries
        ${CMAKE_SOURCE_DIR}/NeuronClient/Sound/QMixer.lib
        ${CMAKE_SOURCE_DIR}/NeuronClient/Netplay/Mplayer.lib
        # Windows system libraries
        winmm
)
