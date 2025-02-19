add_library(${PROJECT_NAME} STATIC)

find_package(XercesC REQUIRED)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PUBLIC XercesC::XercesC)

file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.cc)
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
