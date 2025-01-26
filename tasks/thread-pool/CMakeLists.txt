begin_task()

set_task_sources(
  thread_pool.hpp
  thread_pool.cpp
  queue.hpp

)
add_task_test(
  thread_pool_tests
  wait_group.hpp
  thread_pool_test.cpp
  thread_pool.cpp
)

end_task()
