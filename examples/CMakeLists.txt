function(add_example name)
  add_executable(${name} ${name}.cc)
  target_link_libraries(${name} PRIVATE SpwRmap)
endfunction()

add_example(example_template)
add_example(spw_test)
