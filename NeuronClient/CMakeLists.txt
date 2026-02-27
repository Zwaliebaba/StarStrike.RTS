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

file(GLOB GAMELIB_SRC CONFIGURE_DEPENDS
    "Gamelib/*.c"
    "Gamelib/*.h"
)

file(GLOB WIDGET_SRC CONFIGURE_DEPENDS
    "Widget/*.c"
    "Widget/*.h"
)

# Only C sources – getdxver.cpp is intentionally excluded.
file(GLOB IVIS02_SRC CONFIGURE_DEPENDS
    "ivis02/*.c"
    "ivis02/*.h"
)

add_library (NeuronClient STATIC
    "NeuronClient.c"
    "NeuronClient.h"
    ${FRAMEWORK_SRC}
    ${GAMELIB_SRC}
    ${WIDGET_SRC}
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
)

# PUBLIC: NeuronCore's headers and lib are forwarded transitively to StarStrike.
target_link_libraries (NeuronClient
    PUBLIC
        NeuronCore
)
