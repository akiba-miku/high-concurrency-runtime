#pragma once

#include "runtime/base/noncopyable.h"

#include <cstddef>
#include <string>
#include <vector>

namespace runtime::net {

/**
 * 用户态字节缓冲区管理器
 * ｜prependable | readable | writable | 
 * 0            reader      writer     size
 * [0, reader) : 被读走， 或者预留给prepend的区域
 * [reader_index, writer_index_) : 当前真正可读的数据
 * [writer_index_, buffer_.size()) : 当前还能写入的区域 
 */
class Buffer : public runtime::base::NonCopyable {
public:
    // 预留一段的prepend区域, 协议编码往前补长度字段
    static constexpr std::size_t kCheapPrepend = 8;
    static constexpr std::size_t kInitialSize = 1024;

    explicit Buffer(std::size_t initial_size = kInitialSize);

    std::size_t ReadableBytes() const; // writer_index_ - reader_index_
    std::size_t WritableBytes() const; // buffer_.size() - writer_index_
    std::size_t PrependableBytes() const; // reader_index_;

    // 可读数据的起始地址: begin() + reader_index_
    const char *Peek() const;

    void Retrieve(std::size_t len);
    void RetrieveUntil(const char *end);
    void RetrieveAll();

    std::string RetrieveAsString(std::size_t len);
    std::string RetrieveAllAsString();

    void Append(const char *data, std::size_t len);
    void Append(const std::string &str);

    char *BeginWrite();
    const char *BeginWrite() const;
    void HasWritten(std::size_t len);

    void EnsureWritableBytes(std::size_t len);

    ssize_t ReadFd(int fd, int *saved_errno);
    ssize_t WriteFd(int fd, int *saved_errno);
private:
    char *Begin();
    const char *Begin() const;
    void MakeSpace(std::size_t len);
private:
    // 管理字节
    std::vector<char> buffer_;
    // 索引滑动->管理数据
    std::size_t reader_index_;
    std::size_t writer_index_;
};
}
