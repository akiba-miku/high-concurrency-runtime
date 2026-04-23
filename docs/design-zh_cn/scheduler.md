### 导读

调度层的组件并不完整，只是作者临时起意加的， 后续可能有更多的补充和完善。

### ThreadPool
经典的线程池设计
该文件用了大量的C++高级语法特性， 请阅读同时提前了解一下特性。

核心就几个， 工作线程，任务队列，锁和条件变量来唤醒协作。

模版化线程池实现， 通过可变参数模版和完美转发接受任意类型的任务。 用invoke_result_t 推导返回类型。 packaged_task + future 把异步执行结果返回给调用方， 然后把任务统一包装成 std::function<void()> 无参无返回值放入任务队列。
```cpp
workers_    ← std::vector<jthread>，固定大小
tasks_      ← std::queue<std::function<void()>>，共享任务队列
mutex_ + cv_← 一把锁 + 一个条件变量保护队列
stop_       ← 关机标志
task_count_ ← 当前排队任务数（原子计数）

```

下面是对这份代码`enqueue`函数的解释， 包括作者初学时看这份代码一脸懵逼。


```cpp
template <class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>> {
  using ReturnType = std::invoke_result_t<F, Args...>;

  auto task = std::make_shared<std::packaged_task<ReturnType()>>(
      [f = std::forward<F>(f),
       ... args = std::forward<Args>(args)]() mutable {
        return std::invoke(std::move(f), std::move(args)...);
      });

  std::future<ReturnType> fut = task->get_future();

  {
    std::unique_lock<std::mutex> lock(mutex_);

    if (stop_) {
      throw std::runtime_error("enqueue on stopped ThreadPool");
    }

    tasks_.emplace([task] { (*task)(); });
    ++task_count_;
  }

  cv_.notify_one();

  return fut;
}

```