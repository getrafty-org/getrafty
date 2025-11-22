set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "bin")

# --------------------------------------------------------------------

# Libraries with CMake targets
set(TARGET_LIBS "GTest::gtest_main;GTest::gmock_main")

# System libraries
set(SYSTEM_LIBS "pthread")

# --------------------------------------------------------------------

# Helpers

macro(get_task_target VAR NAME)
  set(${VAR} task_${TOPIC_NAME}_${TASK_NAME}_${NAME})
endmacro()

function(add_task_executable BINARY_NAME)
  set(BINARY_SOURCES ${ARGN})
  add_executable(${BINARY_NAME} ${BINARY_SOURCES} ${TASK_SOURCES})
  target_link_libraries(${BINARY_NAME} PRIVATE ${TARGET_LIBS} ${SYSTEM_LIBS})

  add_dependencies(${BINARY_NAME}
    GTest::gtest_main
    GTest::gmock_main
  )
endfunction()

# --------------------------------------------------------------------

# Prologue

macro(prologue)
  set(TASK_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  string(REPLACE "${CMAKE_SOURCE_DIR}/" "" RELATIVE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

  get_filename_component(TASK_NAME ${TASK_DIR} NAME)
  get_filename_component(TOPIC_DIR ${TASK_DIR} DIRECTORY)
  get_filename_component(TOPIC_NAME ${TOPIC_DIR} NAME)

  status("Topic = '${TOPIC_NAME}', task = '${TASK_NAME}'")

  include_directories(${TASK_DIR})

  set(TEST_LIST "")
  set(TASK_LIBS_LIST "")
endmacro()

# --------------------------------------------------------------------

# Dependencies

macro(task_link_libraries)
  list(APPEND LIBS_LIST ${ARGV})
endmacro()

macro(target_task_link_libraries TARGET_NAME)
  get_task_target(TARGET ${TARGET_NAME})

  # Process library arguments and resolve task-local libraries
  set(RESOLVED_LIBS "")
  foreach(LIB ${ARGN})
    # Check if it's a keyword (PRIVATE, PUBLIC, INTERFACE)
    if(LIB STREQUAL "PRIVATE" OR LIB STREQUAL "PUBLIC" OR LIB STREQUAL "INTERFACE")
      list(APPEND RESOLVED_LIBS ${LIB})
      # Check if it's a task-local library
    elseif(${LIB} IN_LIST TASK_LIBS_LIST)
      get_task_target(RESOLVED_LIB ${LIB})
      list(APPEND RESOLVED_LIBS ${RESOLVED_LIB})
    else()
      # It's an external library, use as-is
      list(APPEND RESOLVED_LIBS ${LIB})
    endif()
  endforeach()

  target_link_libraries(${TARGET} ${RESOLVED_LIBS})
endmacro()

# --------------------------------------------------------------------

# Sources

macro(set_task_sources)
  # Iterate over each argument in ARGV (ARGV is a semicolon-separated list)
  foreach(FILE ${ARGV})
    # Get the file extension
    get_filename_component(EXT "${FILE}" EXT)
  endforeach()

  # Assign the filtered sources to TASK_SOURCES without adding additional prefixing
  set(TASK_SOURCES ${FILTERED_SOURCES} PARENT_SCOPE)
endmacro()


# --------------------------------------------------------------------

# Libraries

function(add_task_library LIB_NAME)
  get_task_target(LIB_TARGET ${LIB_NAME})

  prepend(LIB_SOURCES "${TASK_DIR}/" ${ARGN})
  add_library(${LIB_TARGET} STATIC ${LIB_SOURCES})

  list(APPEND LIBS_LIST ${LIB_TARGET})
  set(LIBS_LIST ${LIBS_LIST} PARENT_SCOPE)

  # Track task-local libraries for automatic resolution
  list(APPEND TASK_LIBS_LIST ${LIB_NAME})
  set(TASK_LIBS_LIST ${TASK_LIBS_LIST} PARENT_SCOPE)
  set_target_properties(
    ${LIB_TARGET}
    PROPERTIES
    LINKER_LANGUAGE CXX
  )
endfunction()

function(add_task_library_dir DIR_NAME)
  # Optional lib target name (dir name by default)
  if(${ARGC} GREATER 1)
    set(LIB_NAME ${ARGV1})
  else()
    set(LIB_NAME ${DIR_NAME})
  endif()

  set(LIB_DIR ${TASK_DIR}/${DIR_NAME})

  get_task_target(LIB_TARGET ${LIB_NAME})
  status("Add task library: ${LIB_TARGET}")

  file(GLOB_RECURSE LIB_CXX_SOURCES ${LIB_DIR}/*.cpp)
  file(GLOB_RECURSE LIB_HEADERS ${LIB_DIR}/*.hpp ${LIB_DIR}/*.ipp)

  get_filename_component(LIB_INCLUDE_DIR "${LIB_DIR}/.." ABSOLUTE)

  if(LIB_CXX_SOURCES)
    add_library(${LIB_TARGET} STATIC ${LIB_CXX_SOURCES} ${LIB_HEADERS})
    target_include_directories(${LIB_TARGET} PRIVATE ${LIB_INCLUDE_DIR})
    target_link_libraries(${LIB_TARGET} PRIVATE ${LIBS_LIST})
  else()
    # header-only library
    add_library(${LIB_TARGET} INTERFACE)
    target_include_directories(${LIB_TARGET} PRIVATE ${LIB_INCLUDE_DIR})
    target_link_libraries(${LIB_TARGET} PRIVATE ${LIBS_LIST})
  endif()

  # Append ${LIB_TARGET to LIBS_LIST
  list(APPEND LIBS_LIST ${LIB_TARGET})
  set(LIBS_LIST ${LIBS_LIST} PARENT_SCOPE)
endfunction()

# --------------------------------------------------------------------

# Custom target

function(add_task_dir_target NAME DIR_NAME)
  get_task_target(TARGET_NAME ${NAME})

  set(TARGET_DIR "${TASK_DIR}/${DIR_NAME}")
  file(GLOB_RECURSE TARGET_CXX_SOURCES ${TARGET_DIR}/*.cpp)

  add_task_executable(${TARGET_NAME} ${TARGET_CXX_SOURCES})
endfunction()

# --------------------------------------------------------------------

# Playground

function(add_task_playground DIR_NAME)
  get_task_target(PLAY_TARGET_NAME ${DIR_NAME})
  status("Add task playground: ${PLAY_TARGET_NAME}")

  add_task_dir_target(playground ${DIR_NAME})
endfunction()

# --------------------------------------------------------------------

# Tests

function(add_task_test BINARY_NAME)
  get_task_target(TEST_NAME ${BINARY_NAME})

  prepend(TEST_SOURCES "${TASK_DIR}/" ${ARGN})
  add_task_executable(${TEST_NAME} ${TEST_SOURCES})

  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

  if(COMMAND add_coverage_to_test)
    add_coverage_to_test(${TEST_NAME})
  endif()

  # Append test to TEST_LIST
  list(APPEND TEST_LIST ${TEST_NAME})
  set(TEST_LIST "${TEST_LIST}" PARENT_SCOPE)
endfunction()

function(add_task_test_dir DIR_NAME)
  # Optional test target name (dir name by default)
  if(${ARGC} GREATER 1)
    set(BINARY_NAME ${ARGV1})
  else()
    set(BINARY_NAME ${DIR_NAME})
  endif()

  get_task_target(TEST_NAME ${BINARY_NAME})

  set(TEST_DIR "${TASK_DIR}/${DIR_NAME}")
  file(GLOB_RECURSE TEST_CXX_SOURCES ${TEST_DIR}/*.cpp)

  add_task_executable(${TEST_NAME} ${TEST_CXX_SOURCES})

  # Register with CTest
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

  # Add coverage generation (defined in Coverage.cmake)
  if(COMMAND add_coverage_to_test)
    add_coverage_to_test(${TEST_NAME})
  endif()

  # Append test to TEST_LIST
  list(APPEND TEST_LIST ${TEST_NAME})
  set(TEST_LIST "${TEST_LIST}" PARENT_SCOPE)
endfunction()

function(add_task_all_tests_target)
  get_task_target(ALL_TESTS_TARGET "run_all_tests")
  run_chain(${ALL_TESTS_TARGET} ${TEST_LIST})
endfunction()

# --------------------------------------------------------------------

# Benchmark

function(add_task_benchmark BINARY_NAME)
  get_task_target(BENCH_NAME ${BINARY_NAME})

  prepend(BENCH_SOURCES "${TASK_DIR}/" ${ARGN})
  add_task_executable(${BENCH_NAME} ${BENCH_SOURCES})
  target_link_libraries(${BENCH_NAME} PRIVATE benchmark)

  if(${TOOL_BUILD})
    get_task_target(RUN_BENCH_TARGET "run_benchmark")
    add_custom_target(${RUN_BENCH_TARGET} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BENCH_NAME})
    add_dependencies(${RUN_BENCH_TARGET} ${BENCH_NAME})
  endif()
endfunction()

# --------------------------------------------------------------------


# Epilogue

function(epilogue)
  if(${TOOL_BUILD})
    add_task_all_tests_target()
  endif()
endfunction()
