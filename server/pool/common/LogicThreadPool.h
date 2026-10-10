#pragma once
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace ndtapp {
// Bounded business executor. Admission is nonblocking so IO callbacks never
// wait for a slow database, device, FFT or file task.
class LogicThreadPool final {
public:
    LogicThreadPool(std::size_t threads, std::size_t capacity);
    ~LogicThreadPool();
    bool TryPost(std::function<void()> task);
    void Stop();
private:
    void Run();
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::function<void()>> tasks_;
    std::vector<std::thread> threads_;
    std::size_t capacity_;
    bool stopped_ = false;
};
}
