cmake_minimum_required(VERSION 3.15)
project(spw_rmap LANGUAGES CXX)

option(SPWRMAP_BUILD_APPS "Build command line applications" ON)
option(SPWRMAP_BUILD_EXAMPLES "Build examples" OFF)
option(SPWRMAP_BUILD_TESTS "Build tests" ON)

add_library(${PROJECT_NAME} STATIC)

add_subdirectory(src)

if(SPWRMAP_BUILD_APPS)
  add_subdirectory(app)
endif()

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
  EXPORT spw_rmapTargets
  INCLUDES
  DESTINATION include
  ARCHIVE DESTINATION lib)

install(
  EXPORT spw_rmapTargets
  FILE spw_rmapTargets.cmake
  NAMESPACE spw_rmap::
  DESTINATION lib/cmake/spw_rmap)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/spw_rmapConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/spw_rmapConfig.cmake
  INSTALL_DESTINATION lib/cmake/spw_rmap)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/spw_rmapConfigVersion.cmake
  VERSION 0.0.1
  COMPATIBILITY SameMajorVersion)

file(GLOB HEADER_FILES ${CMAKE_SOURCE_DIR}/include/spw_rmap/*.hh)
file(GLOB HEADER_FILES_INTERNAL
     ${CMAKE_SOURCE_DIR}/include/spw_rmap/internal/*.hh)

install(FILES ${HEADER_FILES} DESTINATION include/spw_rmap)
install(FILES ${HEADER_FILES_INTERNAL} DESTINATION include/spw_rmap/internal)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/spw_rmapConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/spw_rmapConfigVersion.cmake
        DESTINATION lib/cmake/spw_rmap)
