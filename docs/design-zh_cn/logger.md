### 导读

本篇介绍经典的异步日志双缓冲区。

双缓冲和生产者-消费者模型的异步日志器
前台线程只负责追加日志到内存缓冲区，
后台线程批量落盘，C++20 `jthread + stop_token + condition_variable_any` 管理线程生命周期与同步。

关于文件处理的部分，日志截断和滚动的部分不做解释， 自行阅读即可。

### 为什么需要异步日志?

同步日志进行系统调用`write()`,IO线程阻塞， 高并发吞吐受限。

### async_logger.h

先关注数据结构
```cpp
BufferPtr current_buffer_; // 业务线程写日志的缓冲区， 调用Append往缓冲区写数据
BufferPtr next_buffer_; // 双缓冲， current写满顶不住了， 立刻切备用顶上。
BufferQueue buffers_; // 已满的Buffer队列， 交给后台线程写入。

```

通过`Append`成员函数来看一下双缓冲。
```cpp
// Append 部分片段

lock_guard lk{mutex_};
if (current_buffer_->Append(data, len)) return;  // 快路径：直接写入，释放锁

buffers_.push_back(std::move(current_buffer_));  // 写满了，推入队列
current_buffer_ = std::move(next_buffer_);        // 用备用buffer，无内存分配
current_buffer_->Append(data, len);
cv_.notify_one();                                 // 唤醒后台线程

```

无任何IO， 只需要调用`LogBuffer`内部的 Append写缓冲区即可。
持锁时间极短。

-----


`ThreadFunc` 的双缓冲交换
注意将 业务的缓冲区队列和 内部队列直接交换是重点。
```cpp
// 持锁期间只做指针交换，不做 IO
{
  unique_lock lk{mutex_};
  cv_.wait_for(lk, flush_interval_ms_);    // 超时或被唤醒
  buffers_.push_back(current_buffer_);     // 把当前 buffer 也收走
  buffers_to_write.swap(buffers_);         // swap 后立即释放锁
  // 归还 spare buffer 给前端
}

// 持锁区结束，以下全是 IO，前端线程可以继续并发写
for (auto& buf : buffers_to_write) WriteBuffer(*buf);
FlushFile();
// 把写完的空 buffer 留作 spare1/spare2，下次交换用

```


### logger.h

上层开发调用日志的接口。
RAII风格的流式日志接口， 宏负责注入文件名、行号、函数名并做级别过滤，LogMessage 临时对象负责收集流内容，析构时统一调用 Logger::Log 交给异步日志器
实现:
作者设计了函数宏的日志写法和级别过滤， 后续支持流式输出 代码由`Claude`赞助生成。使用经典单例模式。

注: 这几乎是本项目唯一使用宏魔法的地方， 其它地方都可以用 using, const , constexpr, inine 这些关键字替代。 
```cpp
#define LOG_INFO()                          \
  if (!Logger::Instance().ShouldLog(INFO)) \
    ;                                       \  // ← 级别过滤，不进入构造
  else                                      \
    LogMessage(INFO, __FILE__, __LINE__, __func__).Stream()
//                                          ↑ 析构时才触发 Append


```