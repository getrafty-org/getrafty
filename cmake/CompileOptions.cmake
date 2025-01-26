# Common compile options for C++

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# https://clang.llvm.org/docs/DiagnosticsReference.html
add_compile_options(-Wall -Wextra -Wpedantic -g -fno-omit-frame-pointer)

# Turn warnings into errors
add_compile_options(-Wno-language-extension-token)

add_compile_options(-Wno-error=unused-command-line-argument)

add_compile_options(-gdwarf-4)

# Switch to GCC's libstdc++ to support <ext/random>
add_compile_options(-stdlib=libstdc++)
add_link_options(-stdlib=libstdc++)

# fuse
add_link_options(-lfuse3)

message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
