project(SpwRmap LANGUAGES CXX)

option(SPWRMAP_BUILD_EXAMPLES "Build examples" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)

if(SPWRMAP_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
