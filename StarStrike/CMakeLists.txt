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
        # Windows App SDK / WinRT (OS-inbox since Windows 8)
        WindowsApp
)

# ---------------------------------------------------------------------------
# MakePri: compile .resw string sources → resources.pri
#
# makepri.exe ships with the Windows SDK (typically under
# %WindowsSdkDir%\bin\<version>\x64\makepri.exe).
# We locate it via CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION or fall back to
# a known default; callers can override by setting MAKEPRI_EXE in their
# environment or CMake cache.
# ---------------------------------------------------------------------------
if (NOT MAKEPRI_EXE)
    # Try to find makepri.exe from the Windows SDK
    set(_WINSDK_ROOT "$ENV{WindowsSdkDir}")
    set(_WINSDK_VER  "$ENV{WindowsSdkVersion}")
    if (_WINSDK_ROOT AND _WINSDK_VER)
        string(STRIP "${_WINSDK_VER}" _WINSDK_VER)
        string(REGEX REPLACE "\\\\$" "" _WINSDK_VER "${_WINSDK_VER}")
        set(_MAKEPRI_CANDIDATE "${_WINSDK_ROOT}bin\\${_WINSDK_VER}\\x64\\makepri.exe")
        if (EXISTS "${_MAKEPRI_CANDIDATE}")
            set(MAKEPRI_EXE "${_MAKEPRI_CANDIDATE}" CACHE FILEPATH "Path to makepri.exe")
        endif()
    endif()
    if (NOT MAKEPRI_EXE)
        # Last resort: hope it is on PATH
        set(MAKEPRI_EXE "makepri.exe" CACHE FILEPATH "Path to makepri.exe")
    endif()
endif()

set(RESW_DIR "${CMAKE_SOURCE_DIR}/GameData/strings/en-US")
set(PRI_CONFIG "${CMAKE_SOURCE_DIR}/priconfig.xml")
set(PRI_OUTPUT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources.pri")

# Glob all .resw files so CMake knows about them as inputs
file(GLOB RESW_FILES "${RESW_DIR}/*.resw")

add_custom_command(
    OUTPUT  "${PRI_OUTPUT}"
    COMMAND "${MAKEPRI_EXE}" new
            /pr "${RESW_DIR}"
            /cf "${PRI_CONFIG}"
            /of "${PRI_OUTPUT}"
            /o               # overwrite without prompting
    DEPENDS ${RESW_FILES} "${PRI_CONFIG}"
    COMMENT "Compiling string resources → resources.pri"
    VERBATIM
)

add_custom_target(StarStrike_PRI ALL DEPENDS "${PRI_OUTPUT}")
add_dependencies(StarStrike StarStrike_PRI)
