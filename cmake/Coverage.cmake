if((CMAKE_BUILD_TYPE MATCHES Debug))
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Coverage reporting requires Clang compiler")
  endif()

  message(STATUS "Coverage enabled")

  add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
  add_link_options(-fprofile-instr-generate)

  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/coverage")

  get_filename_component(COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
  get_filename_component(LLVM_BASE "${COMPILER_DIR}" DIRECTORY)

  find_program(LLVM_PROFDATA
    NAMES llvm-profdata
    HINTS "${LLVM_BASE}/bin" "${COMPILER_DIR}"
    REQUIRED)

  find_program(LLVM_COV
    NAMES llvm-cov
    HINTS "${LLVM_BASE}/bin" "${COMPILER_DIR}"
    REQUIRED)

  set(COVERAGE_SCRIPT "${CMAKE_BINARY_DIR}/create-coverage-report.sh")
  file(WRITE ${COVERAGE_SCRIPT} "#!/bin/sh\n")
  file(APPEND ${COVERAGE_SCRIPT} "set -e\n\n")
  file(APPEND ${COVERAGE_SCRIPT} "cd ${CMAKE_BINARY_DIR}\n\n")
  file(APPEND ${COVERAGE_SCRIPT} "# Check if there are any .profraw files\n")
  file(APPEND ${COVERAGE_SCRIPT} "if ! ls coverage/*.profraw 1> /dev/null 2>&1; then\n")
  file(APPEND ${COVERAGE_SCRIPT} "  echo 'No coverage data found. Run tests first'\n")
  file(APPEND ${COVERAGE_SCRIPT} "  exit 0\n")
  file(APPEND ${COVERAGE_SCRIPT} "fi\n\n")
  file(APPEND ${COVERAGE_SCRIPT} "${LLVM_PROFDATA} merge -sparse coverage/*.profraw -o coverage/coverage.profdata\n\n")
  file(APPEND ${COVERAGE_SCRIPT} "# Find all test executables (absolute paths)\n")
  file(APPEND ${COVERAGE_SCRIPT} "TEST_BINARIES=$(find ${CMAKE_BINARY_DIR}/tasks -name 'task_*test' -type f -executable 2>/dev/null)\n\n")
  file(APPEND ${COVERAGE_SCRIPT} "if [ -z \"$TEST_BINARIES\" ]; then\n")
  file(APPEND ${COVERAGE_SCRIPT} "  echo 'No test binaries found'\n")
  file(APPEND ${COVERAGE_SCRIPT} "  exit 0\n")
  file(APPEND ${COVERAGE_SCRIPT} "fi\n\n")
  file(APPEND ${COVERAGE_SCRIPT} "echo ''\n")
  file(APPEND ${COVERAGE_SCRIPT} "${LLVM_COV} report -instr-profile=coverage/coverage.profdata $TEST_BINARIES -ignore-filename-regex='build/_deps/.*'\n")
  file(APPEND ${COVERAGE_SCRIPT} "echo ''\n")
  execute_process(COMMAND chmod +x ${COVERAGE_SCRIPT})

  function(add_coverage_to_test TEST_TARGET)
    if(CMAKE_BUILD_TYPE MATCHES Debug)
      # Set environment variable for when test runs via CTest
      set_tests_properties(${TEST_TARGET} PROPERTIES
        ENVIRONMENT "LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/coverage/${TEST_TARGET}-%p.profraw"
      )
    endif()
  endfunction()

  # Add custom target to generate coverage report
  add_custom_target(coverage-report
    COMMAND ${COVERAGE_SCRIPT}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating coverage report from collected data..."
  )
endif()
