# CMakeLists.txt : StarStrike executable
# Depends on NeuronClient (which transitively brings in NeuronCore).
#
# CONFIGURE_DEPENDS: CMake will re-glob and re-configure automatically
# whenever .c or .h files are added to or removed from this directory.

file(GLOB STARSTRIKE_SRC CONFIGURE_DEPENDS
    "*.c"
    "*.h"
)

add_executable (StarStrike ${STARSTRIKE_SRC})

# PRIVATE: StarStrike is the final executable; nothing consumes it downstream.
target_link_libraries (StarStrike
    PRIVATE
        NeuronClient
)
