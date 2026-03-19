#pragma once

namespace runtime::base {

/**
 * NonCopyable 被继承后，其派生类无法拷贝，但可正常构造和析构。
 */
class NonCopyable {
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;

    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

}  // namespace runtime::base
