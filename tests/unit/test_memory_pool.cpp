#include <gtest/gtest.h>

#include "runtime/memory/memory_pool.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace {

struct TrackedObject {
    inline static std::atomic<int> live_count{0};
    inline static std::atomic<int> ctor_count{0};
    inline static std::atomic<int> dtor_count{0};

    int value;
    std::string payload;

    TrackedObject(int v, std::string p)
        : value(v), payload(std::move(p)) {
        ++live_count;
        ++ctor_count;
    }

    ~TrackedObject() {
        --live_count;
        ++dtor_count;
    }
};

}  // namespace

TEST(MemoryPoolTest, AllocateAndDeallocateReuseSlots) {
    runtime::memory::MemoryPool<int, 4> pool;

    void* a = pool.Allocate();
    void* b = pool.Allocate();

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b);
    EXPECT_EQ(pool.free_count(), 2u);
    EXPECT_EQ(pool.used_count(), 2u);

    pool.Deallocate(a);
    EXPECT_EQ(pool.free_count(), 3u);

    void* c = pool.Allocate();
    EXPECT_EQ(c, a);
    EXPECT_EQ(pool.free_count(), 2u);
}

TEST(MemoryPoolTest, ReturnsNullWhenExhausted) {
    runtime::memory::MemoryPool<int, 2> pool;

    void* a = pool.Allocate();
    void* b = pool.Allocate();
    void* c = pool.Allocate();

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(c, nullptr);
    EXPECT_EQ(pool.free_count(), 0u);
    EXPECT_EQ(pool.used_count(), 2u);
}

TEST(MemoryPoolTest, ConstructAndDestroyManageLifetime) {
    TrackedObject::live_count = 0;
    TrackedObject::ctor_count = 0;
    TrackedObject::dtor_count = 0;

    runtime::memory::MemoryPool<TrackedObject, 2> pool;

    auto* first = pool.Construct(7, "alpha");
    auto* second = pool.Construct(9, "beta");

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(first->value, 7);
    EXPECT_EQ(second->payload, "beta");
    EXPECT_EQ(TrackedObject::live_count.load(), 2);
    EXPECT_EQ(TrackedObject::ctor_count.load(), 2);
    EXPECT_EQ(pool.free_count(), 0u);

    pool.Destroy(first);
    EXPECT_EQ(TrackedObject::live_count.load(), 1);
    EXPECT_EQ(TrackedObject::dtor_count.load(), 1);
    EXPECT_EQ(pool.free_count(), 1u);

    pool.Destroy(second);
    EXPECT_EQ(TrackedObject::live_count.load(), 0);
    EXPECT_EQ(TrackedObject::dtor_count.load(), 2);
    EXPECT_EQ(pool.free_count(), 2u);
}

TEST(MemoryPoolTest, OwnsRecognizesPoolAddresses) {
    runtime::memory::MemoryPool<std::uint64_t, 4> pool;

    void* p = pool.Allocate();
    ASSERT_NE(p, nullptr);

    EXPECT_TRUE(pool.owns(p));

    int stack_value = 0;
    EXPECT_FALSE(pool.owns(&stack_value));
    EXPECT_FALSE(pool.owns(nullptr));

    pool.Deallocate(p);
}

TEST(MemoryPoolTest, ConcurrentAllocateAndDeallocatePreservesCapacity) {
    runtime::memory::MemoryPool<int, 256> pool;
    constexpr int kThreads = 8;
    constexpr int kIterations = 2000;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&pool] {
            std::vector<void*> local;
            local.reserve(32);

            for (int i = 0; i < kIterations; ++i) {
                void* ptr = nullptr;
                while (ptr == nullptr) {
                    ptr = pool.Allocate();
                    if (ptr == nullptr && !local.empty()) {
                        pool.Deallocate(local.back());
                        local.pop_back();
                    }
                }

                local.push_back(ptr);

                if (local.size() >= 16) {
                    pool.Deallocate(local.back());
                    local.pop_back();
                }
            }

            for (void* ptr : local) {
                pool.Deallocate(ptr);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(pool.free_count(), pool.capacity());
    EXPECT_EQ(pool.used_count(), 0u);
}
