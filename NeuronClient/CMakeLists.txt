# CMakeLists.txt : NeuronClient static library
# Depends on NeuronCore.
#
# Subdirectory layout:
#   Framework/  - core OS/platform abstraction (memory, resources, input, ...)
#   Gamelib/    - game-level utilities (animation, hash tables, parser, ...)
#   Widget/     - UI widget system
#   ivis02/     - rendering / image library
#
# CONFIGURE_DEPENDS: CMake will re-glob and re-configure automatically
# whenever files are added to or removed from these directories.

file(GLOB FRAMEWORK_SRC CONFIGURE_DEPENDS
    "Framework/*.cpp"
    "Framework/*.h"
)
# Resource.cpp is obsolete and replaced by FrameResource.cpp; it contains #error.
list(FILTER FRAMEWORK_SRC EXCLUDE REGEX "Framework/Resource\\.cpp$")

file(GLOB GAMELIB_SRC CONFIGURE_DEPENDS
    "Gamelib/*.cpp"
    "Gamelib/*.h"
)

file(GLOB WIDGET_SRC CONFIGURE_DEPENDS
    "Widget/*.cpp"
    "Widget/*.h"
)

file(GLOB SOUND_SRC CONFIGURE_DEPENDS
    "Sound/*.cpp"
    "Sound/*.h"
)

file(GLOB SCRIPT_SRC CONFIGURE_DEPENDS
    "Script/*.cpp"
    "Script/*.h"
)

file(GLOB NETPLAY_SRC CONFIGURE_DEPENDS
    "Netplay/*.cpp"
    "Netplay/*.h"
)

# Sequence sources removed — proprietary RPL video codec (STREAMER.h / WINSTR.LIB)
# is not viable on modern toolchains. SeqDisp.cpp in StarStrike/ provides stubs.
# file(GLOB SEQUENCE_SRC CONFIGURE_DEPENDS
#     "Sequence/*.cpp"
#     "Sequence/*.h"
# )

file(GLOB IVIS02_SRC CONFIGURE_DEPENDS
    "ivis02/*.cpp"
    "ivis02/*.h"
)

add_library (NeuronClient STATIC
    ${FRAMEWORK_SRC}
    ${GAMELIB_SRC}
    ${WIDGET_SRC}
    ${SOUND_SRC}
    ${SCRIPT_SRC}
    ${NETPLAY_SRC}
    ${IVIS02_SRC}
)

# PUBLIC: every consumer of NeuronClient automatically gets all include paths.
target_include_directories (NeuronClient
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Framework
        ${CMAKE_CURRENT_SOURCE_DIR}/Gamelib
        ${CMAKE_CURRENT_SOURCE_DIR}/Widget
        ${CMAKE_CURRENT_SOURCE_DIR}/ivis02
        ${CMAKE_CURRENT_SOURCE_DIR}/Sound
        ${CMAKE_CURRENT_SOURCE_DIR}/Script
        ${CMAKE_CURRENT_SOURCE_DIR}/Netplay
        ${CMAKE_SOURCE_DIR}/DX9/Include
)

# Make lex-generated scanner symbols static to avoid duplicate-symbol link errors
# when multiple *L.cpp files each define input() / unput().
target_compile_definitions (NeuronClient
    PRIVATE
        YY_STATIC
)

# PUBLIC: NeuronCore's headers and lib are forwarded transitively to StarStrike.
target_link_libraries (NeuronClient
    PUBLIC
        NeuronCore
)
