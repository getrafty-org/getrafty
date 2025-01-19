begin_task()

set_task_sources(
        client.hpp
        event_watcher.cpp
        event_watcher.hpp
        logging.cpp
        logging.hpp
        thread_pool.hpp
        thread_pool.cpp
        queue.hpp
        channel.hpp
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


add_task_test(
        client_test
        client_test.cpp
        client.hpp
)

target_compile_options(
        task_tasks_rpc-io_client_test
        PRIVATE
        -Wno-error
)

end_task()
