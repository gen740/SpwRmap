cmake_minimum_required(VERSION 3.27)
project(SpwRmap LANGUAGES CXX)

option(SPWRMAP_BUILD_EXAMPLES "Build examples" OFF)
add_subdirectory(src)

if(SPWRMAP_BUILD_EXAMPLES)
  add_subdirectory(examples)
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
install(FILES ${HEADER_FILES} DESTINATION include/SpwRmap)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/SpwRmapConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/SpwRmapConfigVersion.cmake
        DESTINATION lib/cmake/SpwRmap)
