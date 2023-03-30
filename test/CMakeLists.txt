enable_testing()

include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(config.h.in config.h)

add_executable(test_list
  test_list.c
  simpletest.c
)
target_link_libraries(test_list PRIVATE core)
add_test(NAME test_list COMMAND test_list)

add_executable(test_threadpool
  test_threadpool.cpp
)
target_link_libraries(test_threadpool PRIVATE core GTest::gtest_main)
add_test(NAME test_threadpool COMMAND test_threadpool)
set_tests_properties(test_threadpool PROPERTIES TIMEOUT 10)

