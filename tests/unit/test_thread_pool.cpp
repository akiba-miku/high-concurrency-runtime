#include <gtest/gtest.h>

#include <atomic>

#include "runtime/task/thread_pool.h"

TEST(ThreadPoolTest, ExecutesTasksAndReturnsFutureValues) {
    runtime::task::ThreadPool pool(2);
    std::atomic<int> executed{0};

    auto first = pool.enqueue([&executed] {
        ++executed;
        return 21 + 21;
    });

    auto second = pool.enqueue([&executed](int value) {
        ++executed;
        return value * 2;
    }, 9);

    EXPECT_EQ(first.get(), 42);
    EXPECT_EQ(second.get(), 18);
    EXPECT_EQ(executed.load(), 2);
}
