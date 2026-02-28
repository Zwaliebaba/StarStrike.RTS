# CMakeLists.txt : NeuronClient static library
# Depends on NeuronCore.
#
# Subdirectory layout:
#   Framework/  - core OS/platform abstraction (memory, resources, input, ...)
#   Gamelib/    - game-level utilities (animation, hash tables, parser, ...)
#   Widget/     - UI widget system
#   ivis02/     - rendering / image library (C sources only; .cpp excluded)
#
# CONFIGURE_DEPENDS: CMake will re-glob and re-configure automatically
# whenever files are added to or removed from these directories.

file(GLOB FRAMEWORK_SRC CONFIGURE_DEPENDS
    "Framework/*.c"
    "Framework/*.h"
)
# Resource.c is obsolete and replaced by FrameResource.c; it contains #error.
list(FILTER FRAMEWORK_SRC EXCLUDE REGEX "Framework/Resource\\.c$")

file(GLOB GAMELIB_SRC CONFIGURE_DEPENDS
    "Gamelib/*.c"
    "Gamelib/*.h"
)

file(GLOB WIDGET_SRC CONFIGURE_DEPENDS
    "Widget/*.c"
    "Widget/*.h"
)

file(GLOB SOUND_SRC CONFIGURE_DEPENDS
    "Sound/*.c"
    "Sound/*.h"
)

file(GLOB SCRIPT_SRC CONFIGURE_DEPENDS
    "Script/*.c"
    "Script/*.h"
)

file(GLOB NETPLAY_SRC CONFIGURE_DEPENDS
    "Netplay/*.c"
    "Netplay/*.h"
)

# Sequence sources removed — proprietary RPL video codec (STREAMER.h / WINSTR.LIB)
# is not viable on modern toolchains. SeqDisp.c in StarStrike/ provides stubs.
# file(GLOB SEQUENCE_SRC CONFIGURE_DEPENDS
#     "Sequence/*.c"
#     "Sequence/*.h"
# )

# Only C sources
file(GLOB IVIS02_SRC CONFIGURE_DEPENDS
    "ivis02/*.c"
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
# when multiple *L.c files each define input() / unput().
target_compile_definitions (NeuronClient
    PRIVATE
        YY_STATIC
)

# PUBLIC: NeuronCore's headers and lib are forwarded transitively to StarStrike.
target_link_libraries (NeuronClient
    PUBLIC
        NeuronCore
)
