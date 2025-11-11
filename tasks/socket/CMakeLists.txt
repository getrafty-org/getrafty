begin_task()


add_task_library(
  event_watcher
  event_watcher.hpp
  event_watcher.cpp
)

target_task_link_libraries(
  event_watcher
  PRIVATE
  ttl
)

add_task_library(
  transport
  transport.hpp
  tcp_transport.hpp
  tcp_transport.cpp
  framed_transport.hpp
  framed_transport.cpp
)

target_task_link_libraries(
  transport
  PRIVATE
  event_watcher
  ttl
)

add_task_library(
  socket
  socket.hpp
  socket.cpp
)

target_task_link_libraries(
  socket
  PRIVATE
  event_watcher
  transport
  PUBLIC
  ttl
)

add_task_test(
  socket_test
  socket_test.cpp
)

target_task_link_libraries(
  socket_test
  PRIVATE
  gtest
  socket
  event_watcher
  ttl
)

add_task_test(
  socket_stress_test
  socket_stress_test.cpp
)

target_task_link_libraries(
  socket_stress_test
  PRIVATE
  gtest
  socket
  event_watcher
)


add_task_benchmark(
  socket_bench
  socket_bench.cpp
)

target_task_link_libraries(
  socket_bench
  PRIVATE
  socket
  event_watcher
  transport
)

end_task()
