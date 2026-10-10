#pragma once
#include <cppconn/connection.h>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

namespace ndtapp {
class MySqlPool final {
public:
    class Handle final {
    public:
        Handle() = default;
        Handle(MySqlPool *owner, std::unique_ptr<sql::Connection> connection);
        Handle(Handle &&other) noexcept;
        Handle &operator=(Handle &&other) noexcept;
        Handle(const Handle &) = delete;
        Handle &operator=(const Handle &) = delete;
        ~Handle();
        sql::Connection *operator->() const { return connection_.get(); }
        explicit operator bool() const { return bool(connection_); }
    private:
        MySqlPool *owner_ = nullptr;
        std::unique_ptr<sql::Connection> connection_;
    };
    MySqlPool();
    ~MySqlPool();
    Handle Acquire();
    void Stop();
private:
    friend class Handle;
    std::unique_ptr<sql::Connection> Connect();
    void Release(std::unique_ptr<sql::Connection> connection);
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::unique_ptr<sql::Connection>> idle_;
    std::string host_, user_, password_, database_;
    std::size_t maximum_ = 4, created_ = 0, active_ = 0;
    int acquireMs_ = 3000, connectMs_ = 3000;
    bool stopped_ = false;
};
}
