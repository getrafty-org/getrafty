begin_task()

# thread_pool
add_library(
  thread_pool
  thread_pool.hpp
  thread_pool.cpp
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
  task_tasks_rpc-io_event_watcher_test
  PRIVATE
  event_watcher
  gtest
  folly
  gmock
)
#

# timer
add_library(
  timer
  timer.hpp
  timer.cpp
)

add_task_test(
  timer_test
  timer_test.cpp
)

target_link_libraries(
  task_tasks_rpc-io_timer_test
  PRIVATE
  timer
  gtest
  folly
  gmock
  event_watcher
)
#

set_task_sources(
  client.hpp
  client.cpp
  event_watcher.cpp
  event_watcher.hpp
  logging.cpp
  logging.hpp
  thread_pool.hpp
  thread_pool.cpp
  queue.hpp
  channel.hpp
  clock.hpp
)

#add_task_test(
#  event_watcher_test
#  event_watcher_test.cpp
#  event_watcher.cpp
#  event_watcher.hpp
#  logging.cpp
#  logging.hpp
#  thread_pool.hpp
#  thread_pool.cpp
#  queue.hpp
#)

# client
add_library(
  client
  client.hpp
  client.cpp
)

add_task_test(
  client_test
  client_test.cpp
  client.hpp
  client.cpp
)

target_link_libraries(
  client
  PRIVATE
  folly
  timer
  thread_pool
  event_watcher
)

target_link_libraries(
  task_tasks_rpc-io_client_test
  PRIVATE
  folly
  timer
  thread_pool
  event_watcher
)

target_compile_options(
  task_tasks_rpc-io_client_test
  PRIVATE
  -Wno-error
)
#

end_task()
