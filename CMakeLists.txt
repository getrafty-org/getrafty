cmake_minimum_required(VERSION 3.29)

project(getrafty)

include(cmake/Development.cmake)
include(cmake/Logging.cmake)
include(cmake/Ccache.cmake)
include(cmake/Helpers.cmake)
include(cmake/CheckCompiler.cmake)
include(cmake/CompileOptions.cmake)
include(cmake/Sanitize.cmake)
include(cmake/Platform.cmake)

enable_testing()

include(cmake/Coverage.cmake)

add_subdirectory(third-party)

# This must be included after library so that
# third party code is not affected
include(cmake/Werror.cmake)
include(cmake/Task.cmake)

add_subdirectory(bits)
add_subdirectory(tasks)
