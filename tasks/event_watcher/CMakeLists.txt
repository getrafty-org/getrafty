prologue()

add_task_library(
  event_watcher
  event_watcher.cpp
  event_watcher.hpp
)

target_task_link_libraries(
  event_watcher
  PRIVATE
  bits
)

add_task_test(
  event_watcher_test
  event_watcher_test.cpp
)

target_task_link_libraries(
  event_watcher_test
  PRIVATE
  event_watcher
  bits
  ttl
)

epilogue()
