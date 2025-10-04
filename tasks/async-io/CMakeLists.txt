begin_task()

# thread_pool
add_library(
  thread_pool
  thread_pool.hpp
  thread_pool.cpp
)
target_link_libraries(
  thread_pool
  PRIVATE
  fmt::fmt
)

#

# event_watcher
add_library(
  event_watcher
  event_watcher.cpp
  event_watcher.hpp
)

target_link_libraries(
  event_watcher
  PRIVATE
  fmt::fmt
  thread_pool
)

add_task_test(
  event_watcher_test
  event_watcher_test.cpp
)

target_link_libraries(
  task_tasks_async-io_event_watcher_test
  PRIVATE
  event_watcher
)

#

set_task_sources(
  event_watcher.cpp
  event_watcher.hpp
  thread_pool.hpp
  thread_pool.cpp
  queue.hpp
)

end_task()
