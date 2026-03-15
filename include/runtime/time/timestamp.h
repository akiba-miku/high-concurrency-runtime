#pragma once
/**
 * 对时间点的抽象。
 * 这一层只负责“表示一个时刻”和“格式化输出”，
 * 不负责定时任务调度，后续 Timer 可以复用它。
 */

#include "runtime/base/types.h"

#include <cstdint>

namespace runtime::time {

class Timestamp {
public:
    Timestamp() = default;

    // 使用 Unix Epoch 以来的微秒数构造时间点。
    explicit Timestamp(runtime::base::u64 microseconds_since_epoch)
        : microseconds_since_epoch_(microseconds_since_epoch) {}

    // 获取系统时间。
    static Timestamp now();
    // 返回一个无效时间点，用于占位/初始化。
    static Timestamp invalid();

    // 时间点是否合法。
    bool valid() const;

    // 不同粒度的 Epoch 时间，便于日志、统计和定时器共用。
    runtime::base::u64 microsecondsSinceEpoch() const;
    runtime::base::u64 millisecondsSinceEpoch() const;
    runtime::base::u64 secondsSinceEpoch() const;

    // 机器格式：seconds.microseconds
    runtime::base::String toString() const;
    // 日志格式：YYYY-MM-DD HH:MM:SS[.uuuuuu]
    runtime::base::String toFormattedString(bool show_microseconds = true) const;

    friend bool operator<(const Timestamp& lhs, const Timestamp& rhs);
    friend bool operator==(const Timestamp& lhs, const Timestamp& rhs);

private:
    runtime::base::u64 microseconds_since_epoch_{ 0 };
};

// 返回两个时间点的差值，单位为秒。
double timeDifference(const Timestamp& high, const Timestamp& low);
// 在指定时间点基础上增加秒数，生成新的时间点。
Timestamp addTime(const Timestamp& timestamp, double seconds);

}  // namespace runtime::time
