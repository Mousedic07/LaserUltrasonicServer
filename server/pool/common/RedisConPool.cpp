#include "RedisConPool.h"
#include "ConfigMgr.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <winsock2.h>

namespace ndtapp {
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace {
std::string replyLine(tcp::socket &socket) {
    std::string line;
    for (;;) {
        char c; asio::read(socket, asio::buffer(&c, 1));
        if (c == '\n') break;
        if (c != '\r') line.push_back(c);
        if (line.size() > 1024) throw std::runtime_error("Redis reply too long");
    }
    if (line.empty() || line[0] == '-') throw std::runtime_error("Redis error: " + line);
    return line;
}
}
std::string RedisConPool::Connection::Send(const std::vector<std::string> &parts) {
    std::string command = "*" + std::to_string(parts.size()) + "\r\n";
    for (const auto &part : parts)
        command += "$" + std::to_string(part.size()) + "\r\n" + part + "\r\n";
    asio::write(socket, asio::buffer(command));
    return replyLine(socket);
}
RedisConPool::Handle::Handle(RedisConPool *owner, std::unique_ptr<Connection> connection)
    : owner_(owner), connection_(std::move(connection)) {}
RedisConPool::Handle::Handle(Handle &&other) noexcept
    : owner_(other.owner_), connection_(std::move(other.connection_)) { other.owner_ = nullptr; }
RedisConPool::Handle &RedisConPool::Handle::operator=(Handle &&other) noexcept {
    if (this != &other) {
        if (owner_) owner_->Release(std::move(connection_));
        owner_ = other.owner_; connection_ = std::move(other.connection_); other.owner_ = nullptr;
    }
    return *this;
}
RedisConPool::Handle::~Handle() { if (owner_) owner_->Release(std::move(connection_)); }
RedisConPool::RedisConPool() {
    auto &cfg = ConfigMgr::Instance();
    host_ = cfg.Get("Redis", "Host", "127.0.0.1", "NDT_REDIS_HOST");
    port_ = cfg.GetInt("Redis", "Port", 6379, "NDT_REDIS_PORT");
    password_ = cfg.Get("Redis", "Password", "", "NDT_REDIS_PASSWORD");
    maximum_ = std::size_t(cfg.GetInt("Redis", "PoolSize", 4));
    acquireMs_ = cfg.GetInt("Redis", "AcquireTimeoutMs", 3000);
    if (!maximum_ || maximum_ > 64 || acquireMs_ < 1 || port_ < 1 || port_ > 65535)
        throw std::runtime_error("invalid Redis pool configuration");
}
RedisConPool::~RedisConPool() { Stop(); }
std::unique_ptr<RedisConPool::Connection> RedisConPool::Connect() {
    auto connection = std::make_unique<Connection>();
    connection->socket.connect({asio::ip::make_address(host_), uint16_t(port_)});
    const DWORD timeout = 3000;
    setsockopt(connection->socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char *>(&timeout), sizeof(timeout));
    setsockopt(connection->socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char *>(&timeout), sizeof(timeout));
    if (!password_.empty() && connection->Send({"AUTH", password_}) != "+OK")
        throw std::runtime_error("Redis authentication failed");
    return connection;
}
RedisConPool::Handle RedisConPool::Acquire() {
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(acquireMs_);
    std::unique_lock<std::mutex> lock(mutex_);
    for (;;) {
        if (stopped_) throw std::runtime_error("Redis pool stopped");
        if (!idle_.empty()) {
            auto connection = std::move(idle_.front()); idle_.pop_front(); ++active_;
            lock.unlock();
            try {
                if (connection->Send({"PING"}) != "+PONG") connection = Connect();
                return Handle(this, std::move(connection));
            } catch (...) {
                try { return Handle(this, Connect()); }
                catch (...) { lock.lock(); --created_; --active_; ready_.notify_one(); throw; }
            }
        }
        if (created_ < maximum_) {
            ++created_; ++active_; lock.unlock();
            try { return Handle(this, Connect()); }
            catch (...) { lock.lock(); --created_; --active_; ready_.notify_one(); throw; }
        }
        if (ready_.wait_until(lock, until) == std::cv_status::timeout)
            throw std::runtime_error("Redis connection acquire timeout");
    }
}
void RedisConPool::Release(std::unique_ptr<Connection> connection) {
    std::lock_guard<std::mutex> lock(mutex_);
    --active_;
    if (connection && connection->socket.is_open() && !stopped_)
        idle_.push_back(std::move(connection));
    else --created_;
    ready_.notify_all();
}
void RedisConPool::Stop() {
    std::unique_lock<std::mutex> lock(mutex_);
    stopped_ = true; ready_.notify_all();
    ready_.wait(lock, [this] { return active_ == 0; });
    idle_.clear(); created_ = 0;
}
}
