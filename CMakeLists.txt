cmake_minimum_required(VERSION 3.27)
project(SpwRmap LANGUAGES CXX)

option(SPWRMAP_BUILD_EXAMPLES "Build examples" OFF)
option(SPWRMAP_BUILD_TESTS "Build tests" OFF)

add_library(${PROJECT_NAME} STATIC)

if(SPWRMAP_DOWNLOAD_DEPENDENCIES)
  message(STATUS "Downloading dependencies...")
  include(ExternalProject)

  set(_patch_sh "${CMAKE_BINARY_DIR}/patch_throw_to_noexcept.sh")
  file(
    WRITE "${_patch_sh}"
    [=[
#!/usr/bin/env bash
set -euo pipefail
find . -name '*.hh' -print0 | while IFS= read -r -d "" file; do
  perl -0777 -i -pe '
    s{
      \bthrow
      \s*                     # optional whitespace
      \(
      (                      # capture group for inner content
        (?:
          [^()]*             # non-parens
          (?:\([^()]*\)[^()]*)* # handle nested parens (1 level)
        )
      )
      \)
      (?!\s*;)               # negative lookahead: do not match if followed by ;
    }{}gsx
  ' "$file"
done
]=])

  # 実行属性
  file(
    CHMOD
    "${_patch_sh}"
    PERMISSIONS
    OWNER_READ
    OWNER_WRITE
    OWNER_EXECUTE
    GROUP_READ
    WORLD_READ)

  ExternalProject_Add(
    XMLUtilities
    GIT_REPOSITORY https://github.com/sakuraisoki/XMLUtilities/
    GIT_TAG 20131217_1820
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")
  ExternalProject_Get_Property(XMLUtilities SOURCE_DIR)
  set(XMLUtilities_INCLUDE_DIRS ${SOURCE_DIR}/include)

  ExternalProject_Add(
    CxxUtilities
    GIT_REPOSITORY https://github.com/yuasatakayuki/CxxUtilities.git
    GIT_TAG 20150929_1409
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    PATCH_COMMAND ${CMAKE_COMMAND} -E env bash -lc "${_patch_sh}"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")
  ExternalProject_Get_Property(CxxUtilities SOURCE_DIR)
  set(CxxUtilities_INCLUDE_DIRS ${SOURCE_DIR}/includes)

  ExternalProject_Add(
    SpaceWireRMAPLibrary
    GIT_REPOSITORY https://github.com/yuasatakayuki/SpaceWireRMAPLibrary.git
    GIT_TAG 2.1.0
    PATCH_COMMAND ${CMAKE_COMMAND} -E env bash -lc "${_patch_sh}"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")
  ExternalProject_Get_Property(SpaceWireRMAPLibrary SOURCE_DIR)
  set(SpaceWireRMAPLibrary_INCLUDE_DIRS ${SOURCE_DIR}/includes)

  target_include_directories(
    ${PROJECT_NAME} SYSTEM
    PRIVATE ${SpaceWireRMAPLibrary_INCLUDE_DIRS} ${XMLUtilities_INCLUDE_DIRS}
            ${CxxUtilities_INCLUDE_DIRS})
  add_dependencies(${PROJECT_NAME} XMLUtilities CxxUtilities
                   SpaceWireRMAPLibrary)
endif()

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
