cmake_minimum_required(VERSION 3.20)
project(high_concurrency_runtime VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build example programs" ON)

# 头文件统一从 include/ 作为根目录引入，便于 examples 和 tests 复用。
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

if(BUILD_TESTS)
    enable_testing()
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
