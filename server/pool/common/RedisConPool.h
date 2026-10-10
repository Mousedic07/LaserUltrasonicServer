#pragma once
#include <boost/asio.hpp>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ndtapp {
class RedisConPool final {
public:
    struct Connection {
        boost::asio::io_context io;
        boost::asio::ip::tcp::socket socket{io};
        std::string Send(const std::vector<std::string> &parts);
    };
    class Handle final {
    public:
        Handle() = default;
        Handle(RedisConPool *owner, std::unique_ptr<Connection> connection);
        Handle(Handle &&other) noexcept;
        Handle &operator=(Handle &&other) noexcept;
        Handle(const Handle &) = delete;
        Handle &operator=(const Handle &) = delete;
        ~Handle();
        Connection *operator->() const { return connection_.get(); }
    private:
        RedisConPool *owner_ = nullptr;
        std::unique_ptr<Connection> connection_;
    };
    RedisConPool();
    ~RedisConPool();
    Handle Acquire();
    void Stop();
private:
    friend class Handle;
    std::unique_ptr<Connection> Connect();
    void Release(std::unique_ptr<Connection> connection);
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::unique_ptr<Connection>> idle_;
    std::string host_, password_;
    int port_ = 6379, acquireMs_ = 3000;
    std::size_t maximum_ = 4, created_ = 0, active_ = 0;
    bool stopped_ = false;
};
}
