begin_task()

set_task_sources(flux_capacitor.hpp flux_capacitor.cpp)

add_task_test(flux_capacitor_test flux_capacitor_test.cpp flux_capacitor.hpp flux_capacitor.cpp)

end_task()
