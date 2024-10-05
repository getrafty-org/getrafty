cmake_minimum_required(VERSION 3.20)

project(getrafty)

include(cmake/Logging.cmake)
include(cmake/Ccache.cmake)
include(cmake/Helpers.cmake)
include(cmake/CheckCompiler.cmake)
include(cmake/CompileOptions.cmake)
include(cmake/Sanitize.cmake)
include(cmake/Platform.cmake)
include(cmake/Development.cmake)

add_subdirectory(third-party)

# This must be included after library so that
# third party code is not affected
include(cmake/Werror.cmake)
include(cmake/Task.cmake)

add_subdirectory(tasks)
