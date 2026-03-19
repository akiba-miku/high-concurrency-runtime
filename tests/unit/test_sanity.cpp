#include <gtest/gtest.h>

// CMakeLists.txt 的 首次测试。
TEST(SanityTest, Basic) {
    EXPECT_EQ(1 + 1, 2);
}