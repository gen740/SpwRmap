cmake_minimum_required(VERSION 3.15)
project(SpwRmap LANGUAGES CXX)

option(SPWRMAP_BUILD_EXAMPLES "Build examples" OFF)
option(SPWRMAP_BUILD_TESTS "Build tests" OFF)

add_library(${PROJECT_NAME} STATIC)

add_subdirectory(src)

if(SPWRMAP_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(SPWRMAP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(SPWRMAP_BUILD_PYTHON_BINDINGS)
  add_subdirectory(bindings/python)
endif()

include(CMakePackageConfigHelpers)

install(
  TARGETS ${PROJECT_NAME}
  EXPORT SpwRmapTargets
  INCLUDES
  DESTINATION include
  ARCHIVE DESTINATION lib)

install(
  EXPORT SpwRmapTargets
  FILE SpwRmapTargets.cmake
  NAMESPACE SpwRmap::
  DESTINATION lib/cmake/SpwRmap)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/SpwRmapConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/SpwRmapConfig.cmake
  INSTALL_DESTINATION lib/cmake/SpwRmap)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/SpwRmapConfigVersion.cmake
  VERSION 0.0.1
  COMPATIBILITY SameMajorVersion)

file(GLOB HEADER_FILES ${CMAKE_SOURCE_DIR}/include/SpwRmap/*.hh)
file(GLOB HEADER_FILES_INTERNAL
     ${CMAKE_SOURCE_DIR}/include/SpwRmap/internal/*.hh)

install(FILES ${HEADER_FILES} DESTINATION include/SpwRmap)
install(FILES ${HEADER_FILES_INTERNAL} DESTINATION include/SpwRmap/internal)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/SpwRmapConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/SpwRmapConfigVersion.cmake
        DESTINATION lib/cmake/SpwRmap)
