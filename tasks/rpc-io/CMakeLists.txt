begin_task()

set_task_sources(
        event_watcher.cpp
        event_watcher.hpp
        logging.cpp
        logging.hpp
        thread_pool.hpp
        thread_pool.cpp
        queue.hpp
)

add_task_test(
        event_watcher_test
        event_watcher_test.cpp
        event_watcher.cpp
        event_watcher.hpp
        logging.cpp
        logging.hpp
        thread_pool.hpp
        thread_pool.cpp
        queue.hpp
)

end_task()
