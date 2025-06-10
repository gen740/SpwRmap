add_library(${PROJECT_NAME} STATIC)

find_package(XercesC REQUIRED)

target_include_directories(
  ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
                         $<INSTALL_INTERFACE:include>)

target_link_libraries(${PROJECT_NAME} PRIVATE XercesC::XercesC)

file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.cc)

target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
target_compile_options(${PROJECT_NAME} PRIVATE -Werror -Wall -Wextra -Wpedantic)
