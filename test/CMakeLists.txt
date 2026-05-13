file(GLOB_RECURSE test_sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS *.cpp)
create_test_sourcelist(driver_sources slate_tests_driver.cpp ${test_sources})

add_executable(test_slate ${driver_sources})
target_link_libraries(test_slate PRIVATE slate::slate)

foreach(test ${test_sources})
  get_filename_component(test_name ${test} NAME_WE)
  add_test(NAME ${test_name} COMMAND test_slate ${test_name})
endforeach()
