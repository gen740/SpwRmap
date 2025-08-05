cmake_minimum_required(VERSION 3.27)

find_package(GTest REQUIRED)

add_executable(spwrmap_tests
  test_spwrmap_base.cc
  test_legacy_spwrmap.cc
  test_dummy_spwrmap.cc
  test_target_node.cc
)

target_link_libraries(spwrmap_tests
  PRIVATE
    SpwRmap
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(spwrmap_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

include(GoogleTest)
gtest_discover_tests(spwrmap_tests)