prologue()

add_task_library(
  queue
  queue.hpp
)

target_task_link_libraries(
  queue
  PRIVATE
  bits
)

add_task_test(
  queue_test
  queue_test.cpp
)

target_task_link_libraries(
  queue_test
  PRIVATE
  queue
  bits
)

add_task_benchmark(
  queue_bench
  queue_bench.cpp
)

target_task_link_libraries(
  queue_bench
  PRIVATE
  queue
  bits
)

epilogue()
