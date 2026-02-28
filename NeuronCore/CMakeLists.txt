# CMakeLists.txt : NeuronCore static library
# No upstream dependencies - this is the foundation layer.

add_library (NeuronCore STATIC
    "NeuronCore.cpp"
    "NeuronCore.h"
)

# Export the include directory so every consumer that links NeuronCore
# automatically gets the header search path via CMake's transitive
# dependency mechanism (no manual include_directories needed upstream).
target_include_directories (NeuronCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
