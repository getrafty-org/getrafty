include(FetchContent)

# Memory allocation

if((CMAKE_BUILD_TYPE MATCHES Release))
    status("FetchContent: mimalloc")

    FetchContent_Declare(
            mimalloc
            GIT_REPOSITORY https://github.com/microsoft/mimalloc
            GIT_TAG v2.1.2
    )
    FetchContent_MakeAvailable(mimalloc)

endif()

# --------------------------------------------------------------------

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

# --------------------------------------------------------------------

status("FetchContent: simde")

FetchContent_Declare(
    simde
    GIT_REPOSITORY https://github.com/simd-everywhere/simde.git
    GIT_TAG v0.8.2
)
FetchContent_MakeAvailable(simde)

add_library(simde INTERFACE)
target_include_directories(simde INTERFACE ${simde_SOURCE_DIR})

# --------------------------------------------------------------------

status("FetchContent: benchmark")

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
FetchContent_MakeAvailable(benchmark)

# --------------------------------------------------------------------

status("FetchContent: flamegraph")

FetchContent_Declare(
    flamegraph
    GIT_REPOSITORY https://github.com/brendangregg/FlameGraph.git
    GIT_TAG master
)
FetchContent_MakeAvailable(flamegraph)

# --------------------------------------------------------------------
