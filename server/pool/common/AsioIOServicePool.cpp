#include "AsioIOServicePool.h"
#include <algorithm>

AsioIOServicePool::AsioIOServicePool(std::size_t size) {
    size = std::max<std::size_t>(1, size);
    for (std::size_t i = 0; i < size; ++i) {
        contexts_.emplace_back(new boost::asio::io_context);
        work_.emplace_back(new Work(boost::asio::make_work_guard(*contexts_.back())));
    }
    for (std::size_t i = 0; i < size; ++i) {
        threads_.emplace_back([this, i]() { contexts_[i]->run(); });
    }
}
AsioIOServicePool::~AsioIOServicePool() { Stop(); }
boost::asio::io_context &AsioIOServicePool::GetIOService() {
    return *contexts_[next_.fetch_add(1) % contexts_.size()];
}
void AsioIOServicePool::Stop() {
    if (stopped_.exchange(true)) return;
    work_.clear();
    for (auto &context : contexts_) context->stop();
    for (auto &thread : threads_) if (thread.joinable()) thread.join();
}
