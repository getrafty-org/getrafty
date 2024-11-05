add_subdirectory(wheels)

begin_task()
set_task_sources(
        event_watcher.cpp
        event_watcher.hpp
)

task_link_libraries(
        wheels
)

add_task_test(event_watcher_test event_watcher_test.cpp)

end_task()
