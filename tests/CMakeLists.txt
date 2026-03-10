include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

# Linux/macOS 下一般无所谓，但保留也没问题
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_executable(runtime_unit_tests
    unit/test_sanity.cpp
)

target_link_libraries(runtime_unit_tests
    PRIVATE
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(runtime_unit_tests)