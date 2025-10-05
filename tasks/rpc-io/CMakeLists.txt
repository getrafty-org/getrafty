begin_task()

add_library(
  task_rpc_io_thread_pool
  thread_pool.hpp
  thread_pool.cpp
  queue.hpp
)

target_link_libraries(
  task_rpc_io_thread_pool
  PRIVATE
  fmt::fmt
)

add_library(
  task_rpc_io_event_watcher
  event_watcher.hpp
  event_watcher.cpp
)

target_link_libraries(
  task_rpc_io_event_watcher
  PRIVATE
  fmt::fmt
)

add_library(
  task_rpc_io_socket
  socket.hpp
)

set_target_properties(
  task_rpc_io_socket
  PROPERTIES
  LINKER_LANGUAGE CXX
)

add_library(
  task_rpc_io_tcp_socket
  tcp_socket.hpp
)

set_target_properties(
  task_rpc_io_tcp_socket
  PROPERTIES
  LINKER_LANGUAGE CXX
)

target_link_libraries(
  task_rpc_io_tcp_socket
  PRIVATE
  task_rpc_io_socket
  task_rpc_io_event_watcher
)

add_library(
  task_rpc_io_transport
  transport.hpp
)

set_target_properties(
  task_rpc_io_transport
  PROPERTIES
  LINKER_LANGUAGE CXX
)

target_link_libraries(
  task_rpc_io_transport
  PRIVATE
  task_rpc_io_socket
)

add_library(
  task_rpc_io_channel
  channel.hpp
)

set_target_properties(
  task_rpc_io_channel
  PROPERTIES
  LINKER_LANGUAGE CXX
)

target_link_libraries(
  task_rpc_io_channel
  PRIVATE
  task_rpc_io_transport
  task_rpc_io_thread_pool
)

add_library(
  task_rpc_io_server
  server.hpp
)

set_target_properties(
  task_rpc_io_server
  PROPERTIES
  LINKER_LANGUAGE CXX
)

target_link_libraries(
  task_rpc_io_server
  PRIVATE
  task_rpc_io_socket
  task_rpc_io_thread_pool
)

add_task_test(
  tcp_socket_test
  tcp_socket_test.cpp
)

target_link_libraries(
  task_tasks_rpc-io_tcp_socket_test
  PRIVATE
  gtest
  task_rpc_io_tcp_socket
  task_rpc_io_event_watcher
)

end_task()
