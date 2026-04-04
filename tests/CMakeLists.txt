include(CTest)

add_executable(memory_pool_smoke_test
    unit/test_memory_pool_smoke.cpp
)

target_include_directories(memory_pool_smoke_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(NAME memory_pool_smoke_test COMMAND memory_pool_smoke_test)

add_executable(object_pool_smoke_test
    unit/test_object_pool_smoke.cpp
)

target_include_directories(object_pool_smoke_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(NAME object_pool_smoke_test COMMAND object_pool_smoke_test)

add_executable(logger_smoke_test
    unit/test_logger_smoke.cpp
)

target_include_directories(logger_smoke_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(logger_smoke_test
    PRIVATE
        runtime_foundation
)

add_test(NAME logger_smoke_test COMMAND logger_smoke_test)

find_package(GTest CONFIG QUIET)
if(GTest_FOUND)
    include(GoogleTest)

    add_executable(runtime_unit_tests
        unit/test_logger.cpp
        unit/test_memory_pool.cpp
        unit/test_object_pool.cpp
        unit/test_sanity.cpp
        unit/test_thread_pool.cpp
    )

    target_link_libraries(runtime_unit_tests
        PRIVATE
            GTest::gtest_main
            runtime_task
    )

    gtest_discover_tests(runtime_unit_tests)
else()
    message(STATUS "GTest not found; skipping gtest-based unit tests.")
endif()
