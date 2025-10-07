begin_task()

add_library(
  queue
  queue.hpp
)

target_link_libraries(
  queue
  PRIVATE
  fmt::fmt
)

set_target_properties(
  queue
  PROPERTIES 
  LINKER_LANGUAGE CXX
)

add_library(
  tp
  thread_pool.hpp
  thread_pool.cpp
)

target_link_libraries(
  tp
  PRIVATE
  queue
  fmt::fmt
)

add_task_test(
  thread_pool_tests
  wait_group.hpp
  thread_pool_test.cpp
  thread_pool.cpp
)

target_link_libraries(
  task_tasks_thread-pool_thread_pool_tests
  PRIVATE
  tp
)

add_task_test(
  queue_tests
  wait_group.hpp
  queue_test.cpp
)

target_link_libraries(
  task_tasks_thread-pool_queue_tests
  PRIVATE
  queue
  fmt::fmt
)

end_task()
