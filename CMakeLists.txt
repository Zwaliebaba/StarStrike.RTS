# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
# 3.12 required for CONFIGURE_DEPENDS on file(GLOB ...) calls.
cmake_minimum_required (VERSION 3.12)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Declare the project as C++ only.
project ("StarStrike" CXX)

# Enforce C++23 globally for all targets.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include sub-projects in dependency order:
#   NeuronCore  (no dependencies)
#   NeuronClient (depends on NeuronCore)
#   StarStrike  (depends on NeuronClient)
add_subdirectory ("NeuronCore")
add_subdirectory ("NeuronClient")
add_subdirectory ("StarStrike")
