# Enable testing
enable_testing()
include(GoogleTest)

# Wheels library
add_library(wheels
        logging.cpp
        thread_pool.cpp
        wait_group.hpp
        queue.hpp
        cpu_timer.hpp
)

# Queue tests
add_executable(queue_test queue_test.cpp)
target_link_libraries(queue_test PRIVATE wheels GTest::gtest_main)
gtest_discover_tests(queue_test)

# ThreadPool tests
add_executable(thread_pool_test thread_pool_test.cpp)
target_link_libraries(thread_pool_test PRIVATE wheels GTest::gtest_main)
gtest_discover_tests(thread_pool_test)