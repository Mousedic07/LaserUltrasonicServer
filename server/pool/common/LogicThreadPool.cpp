#include "LogicThreadPool.h"
#include <algorithm>
#include <iostream>

namespace ndtapp {
LogicThreadPool::LogicThreadPool(std::size_t threads, std::size_t capacity)
    : capacity_(std::max<std::size_t>(1, capacity)) {
    for (std::size_t i = 0; i < std::max<std::size_t>(1, threads); ++i)
        threads_.emplace_back([this] { Run(); });
}
LogicThreadPool::~LogicThreadPool() { Stop(); }
bool LogicThreadPool::TryPost(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_ || tasks_.size() >= capacity_) return false;
    tasks_.push_back(std::move(task)); ready_.notify_one(); return true;
}
void LogicThreadPool::Run() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait(lock, [this] { return stopped_ || !tasks_.empty(); });
            if (tasks_.empty()) return;
            task = std::move(tasks_.front()); tasks_.pop_front();
        }
        try { task(); }
        catch (const std::exception &e) { std::cerr << "LogicThreadPool task: " << e.what() << '\n'; }
    }
}
void LogicThreadPool::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) return;
        stopped_ = true;
    }
    ready_.notify_all();
    for (auto &thread : threads_) if (thread.joinable()) thread.join();
}
}
