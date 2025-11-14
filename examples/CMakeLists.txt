file(GLOB SPW_EXAMPLES CONFIGURE_DEPENDS *.cc)


foreach(example ${SPW_EXAMPLES})
  get_filename_component(example_name ${example} NAME_WE)
  add_executable(${example_name} ${example})
  target_link_libraries(${example_name} PRIVATE spw_rmap)
  target_compile_features(${example_name} PRIVATE cxx_std_23)
endforeach()
