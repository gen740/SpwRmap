find_package(pybind11 CONFIG REQUIRED)
find_package(
  Python3
  COMPONENTS Interpreter
  REQUIRED)

pybind11_add_module(pyspw_rmap MODULE src/pyspw_rmap.cc)
target_compile_features(pyspw_rmap PRIVATE cxx_std_23)
target_link_libraries(pyspw_rmap PRIVATE ${PROJECT_NAME})

set(PYSPW_RMAP_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/pyspw_rmap")

set_target_properties(
  pyspw_rmap
  PROPERTIES OUTPUT_NAME "_core"
             LIBRARY_OUTPUT_DIRECTORY "${PYSPW_RMAP_BUILD_DIR}"
             RUNTIME_OUTPUT_DIRECTORY "${PYSPW_RMAP_BUILD_DIR}")

add_custom_command(
  TARGET pyspw_rmap
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/pyspw_rmap/__init__.py"
    "${PYSPW_RMAP_BUILD_DIR}")

set(PYSPW_RMAP_STUB_OUTPUT "${PYSPW_RMAP_BUILD_DIR}/_core.pyi")

add_custom_command(
  OUTPUT "${PYSPW_RMAP_STUB_OUTPUT}"
  COMMAND ${Python3_EXECUTABLE} -m pybind11_stubgen pyspw_rmap._core -o
          ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Generating Python stub files for raw_rmap module"
  DEPENDS pyspw_rmap
  VERBATIM)

add_custom_target(gen_stubs ALL DEPENDS "${PYSPW_RMAP_STUB_OUTPUT}")

install(
  TARGETS pyspw_rmap
  LIBRARY DESTINATION pyspw_rmap
  RUNTIME DESTINATION pyspw_rmap)
install(FILES ${PYSPW_RMAP_BUILD_DIR}/__init__.py DESTINATION pyspw_rmap)
install(FILES ${PYSPW_RMAP_BUILD_DIR}/_core.pyi DESTINATION pyspw_rmap)
