begin_task()

set_task_sources(
  myfs_server.hpp
  myfs_server.cpp

)
add_task_test(
  myfs_server_test
  myfs_server_test.cpp
  myfs_server.hpp
  myfs_server.cpp
)

end_task()
