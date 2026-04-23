### 导读

本篇 介绍 `include/runtime/base`, `src/base` 的部分

### NonCopyable

设计理念非常直接， 继承它的类自动禁止拷贝， 这样不用为每个禁用拷贝的类单独写一份代码。

语法:
 这个类的目的是用来限制语义， 通过继承父类。 父类禁止拷贝那么对于子类有隐式限制。 (子类必须先为父类准备好配置工作)
 这个类本身不是用来实例化的， 故设为 protected 权限。

```cpp
class NonCopyable {
protected:
  NonCopyable() = default;          // 允许子类构造
  ~NonCopyable() = default;         // 非虚析构（基类不直接持有多态对象）
  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;
};

```

项目中的`EventLoop`, `Channel`, `AsyncLogger`, `LogMessage`. 因为他们持有某些特定资源， (fd, 线程, 锁)， 独占所有权， 禁止拷贝。


### CurrentThread - 线程IO的零缓存成本

`include/runtime/base/current_thread.h` 
它存在的目的就是服务与线程绑定的类， 比如`EventLoop`, 一线程一事件循环。

`thread_local int t_cached_tid = 0`, thread_local 是线程局部存储， 每个线程一份。
因为若是写在全局或者某个命名空间内部， 所有线程都会共享， 而线程局部存储相当于缓存。
避免每次都进行系统调用。

真正执行系统调用的在`src/base/current_thread.cpp`

```cpp
namespace runtime::base {

thread_local int t_cached_tid = 0;

void cacheTid() {
  if (t_cached_tid == 0) {
    // Query the kernel once per thread and keep the result in TLS so later
    // tid() calls can use the cached value directly.
    t_cached_tid = static_cast<int>(::syscall(SYS_gettid));
  }
}

}  // namespace runtime::base

```

还有一处 
`__built_expect(expr, 0)` 告诉编译器Clanged 分支极少成立。
第一次tid为0走慢路径去系统调用拿到自己的tid, 
后续走快路径直接读线程局部副本。
```cpp
inline int tid() {
  if (__builtin_expect(t_cached_tid == 0, 0)) {
    cacheTid();
  }
  return t_cached_tid;
}

```

### Timestamp


