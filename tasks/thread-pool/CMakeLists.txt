begin_task()

add_task_library(
  queue
  queue.hpp
)

target_task_link_libraries(
  queue
  PRIVATE
  ttl
)

add_task_library(
  thread_pool
  thread_pool.hpp
  thread_pool.cpp
)

target_task_link_libraries(
  thread_pool
  PUBLIC
  queue
  ttl
)

add_task_test(
  thread_pool_tests
  wait_group.hpp
  thread_pool_test.cpp
  thread_pool.cpp
)

target_task_link_libraries(
  thread_pool_tests
  PRIVATE
  thread_pool
  ttl
)

add_task_test(
  queue_tests
  wait_group.hpp
  queue_test.cpp
)

target_task_link_libraries(
  queue_tests
  PRIVATE
  queue
  ttl
)

end_task()
