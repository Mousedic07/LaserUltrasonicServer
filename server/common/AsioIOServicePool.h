#pragma once
#include <boost/asio.hpp>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

// Adapted from llfcchat/server/ChatServer/AsioIOServicePool.
// The original round-robin pool is retained with bounded size and safe shutdown.
class AsioIOServicePool {
public:
    explicit AsioIOServicePool(std::size_t size);
    ~AsioIOServicePool();
    AsioIOServicePool(const AsioIOServicePool &) = delete;
    AsioIOServicePool &operator=(const AsioIOServicePool &) = delete;
    boost::asio::io_context &GetIOService();
    void Stop();
private:
    using Work = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
    std::vector<std::unique_ptr<boost::asio::io_context>> contexts_;
    std::vector<std::unique_ptr<Work>> work_;
    std::vector<std::thread> threads_;
    std::atomic<std::size_t> next_{0};
    std::atomic<bool> stopped_{false};
};
