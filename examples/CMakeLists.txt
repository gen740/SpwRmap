file(GLOB SPW_EXAMPLES CONFIGURE_DEPENDS *.cc)


foreach(example ${SPW_EXAMPLES})
  get_filename_component(example_name ${example} NAME_WE)
  add_executable(${example_name} ${example})
  target_link_libraries(${example_name} PRIVATE SpwRmap)
endforeach()
