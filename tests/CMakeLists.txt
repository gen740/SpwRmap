cmake_minimum_required(VERSION 3.27)

find_package(GTest REQUIRED)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS *.cc)

add_executable(spwrmap_tests ${TEST_SOURCES})

target_link_libraries(spwrmap_tests PRIVATE spw_rmap GTest::gtest
                                            GTest::gtest_main)

target_include_directories(spwrmap_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

include(GoogleTest)
gtest_discover_tests(spwrmap_tests)
