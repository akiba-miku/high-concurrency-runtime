cmake_minimum_required(VERSION 3.20)
project(high_concurrency_runtime VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build example programs" ON)

find_package(Threads REQUIRED)

add_library(runtime_foundation STATIC
    src/base/current_thread.cpp
    src/log/async_logger.cpp
    src/log/logger.cpp
    src/time/timestamp.cpp
)

target_include_directories(runtime_foundation
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(runtime_foundation
    PUBLIC
        Threads::Threads
)

target_compile_features(runtime_foundation PUBLIC cxx_std_20)

add_library(runtime_task STATIC
    src/task/thread_pool.cpp
    src/task/task.cpp
    src/task/scheduler.cpp
)

target_include_directories(runtime_task
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(runtime_task
    PUBLIC
        runtime_foundation
        Threads::Threads
)

target_compile_features(runtime_task PUBLIC cxx_std_20)

add_library(runtime_net STATIC
    src/net/acceptor.cpp
    src/net/buffer.cpp
    src/net/channel.cpp
    src/net/epoll_poller.cpp
    src/net/event_loop.cpp
    src/net/event_loop_thread.cpp
    src/net/event_loop_thread_pool.cpp
    src/net/inet_address.cpp
    src/net/net_utils.cpp
    src/net/poller.cpp
    src/net/socket.cpp
    src/net/tcp_connection.cpp
    src/net/tcp_server.cpp
    src/net/timer.cpp
    src/net/timer_queue.cpp
)

target_include_directories(runtime_net
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(runtime_net
    PUBLIC
        runtime_foundation
        Threads::Threads
)

target_compile_features(runtime_net PUBLIC cxx_std_20)

add_library(runtime_http STATIC
    src/http/http_types.cpp
    src/http/http_request.cpp
    src/http/http_context.cpp
    src/http/http_response.cpp
    src/http/router.cpp
    src/http/http_server.cpp
)

target_include_directories(runtime_http
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(runtime_http
    PUBLIC
        runtime_net
        runtime_task
        runtime_foundation
        Threads::Threads
)

target_compile_features(runtime_http PUBLIC cxx_std_20)

add_library(runtime_trace STATIC
    src/trace/trace_id.cpp
)

target_include_directories(runtime_trace
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(runtime_trace
    PUBLIC
        runtime_foundation
)

target_compile_features(runtime_trace PUBLIC cxx_std_20)

add_library(runtime_config STATIC
    src/config/config.cpp
)

target_include_directories(runtime_config
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(runtime_config
    PUBLIC
        runtime_foundation
)

target_compile_features(runtime_config PUBLIC cxx_std_20)

add_library(runtime_metrics STATIC
    src/metrics/registry.cpp
)

target_include_directories(runtime_metrics
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(runtime_metrics
    PUBLIC
        runtime_foundation
)

target_compile_features(runtime_metrics PUBLIC cxx_std_20)

add_library(runtime_health STATIC
    src/health/health_handler.cpp
)

target_include_directories(runtime_health
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(runtime_health
    PUBLIC
        runtime_http
        runtime_metrics
)

target_compile_features(runtime_health PUBLIC cxx_std_20)

add_library(runtime_inference STATIC
    src/inference/llama_engine.cpp
    src/inference/batch_scheduler.cpp
)

target_include_directories(runtime_inference
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(runtime_inference
    PUBLIC
        runtime_net
        runtime_task
        runtime_foundation
)

target_compile_features(runtime_inference PUBLIC cxx_std_20)

add_library(runtime_api STATIC
    src/api/sse_writer.cpp
    src/api/chat_handler.cpp
    src/api/models_handler.cpp
)

target_include_directories(runtime_api
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(runtime_api
    PUBLIC
        runtime_http
        runtime_inference
        runtime_metrics
)

target_compile_features(runtime_api PUBLIC cxx_std_20)

if(BUILD_TESTS)
    enable_testing()
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
