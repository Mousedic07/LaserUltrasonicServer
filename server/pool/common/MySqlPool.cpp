#include "MySqlPool.h"
#include "ConfigMgr.h"
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <algorithm>
#include <stdexcept>

namespace ndtapp {
MySqlPool::Handle::Handle(MySqlPool *owner, std::unique_ptr<sql::Connection> connection)
    : owner_(owner), connection_(std::move(connection)) {}
MySqlPool::Handle::Handle(Handle &&other) noexcept
    : owner_(other.owner_), connection_(std::move(other.connection_)) { other.owner_ = nullptr; }
MySqlPool::Handle &MySqlPool::Handle::operator=(Handle &&other) noexcept {
    if (this != &other) {
        if (owner_) owner_->Release(std::move(connection_));
        owner_ = other.owner_; connection_ = std::move(other.connection_); other.owner_ = nullptr;
    }
    return *this;
}
MySqlPool::Handle::~Handle() { if (owner_) owner_->Release(std::move(connection_)); }
MySqlPool::MySqlPool() {
    auto &cfg = ConfigMgr::Instance();
    host_ = cfg.Get("MySQL", "Host", "127.0.0.1", "NDT_MYSQL_HOST") + ":" +
        std::to_string(cfg.GetInt("MySQL", "Port", 3306, "NDT_MYSQL_PORT"));
    user_ = cfg.Get("MySQL", "User", "ndt", "NDT_MYSQL_USER");
    password_ = cfg.Get("MySQL", "Password", "", "NDT_MYSQL_PASSWORD");
    database_ = cfg.Get("MySQL", "Database", "ndt", "NDT_MYSQL_DATABASE");
    maximum_ = std::size_t(cfg.GetInt("MySQL", "PoolSize", 4));
    acquireMs_ = cfg.GetInt("MySQL", "AcquireTimeoutMs", 3000);
    connectMs_ = cfg.GetInt("MySQL", "ConnectTimeoutMs", 3000);
    if (!maximum_ || maximum_ > 64 || acquireMs_ < 1 || connectMs_ < 1)
        throw std::runtime_error("invalid MySQL pool configuration");
}
MySqlPool::~MySqlPool() { Stop(); }
std::unique_ptr<sql::Connection> MySqlPool::Connect() {
    sql::ConnectOptionsMap options;
    options[OPT_HOSTNAME] = sql::SQLString("tcp://" + host_);
    options[OPT_USERNAME] = sql::SQLString(user_);
    options[OPT_PASSWORD] = sql::SQLString(password_);
    options[OPT_SCHEMA] = sql::SQLString(database_);
    options[OPT_CONNECT_TIMEOUT] = std::max(1, (connectMs_ + 999) / 1000);
    return std::unique_ptr<sql::Connection>(sql::mysql::get_mysql_driver_instance()->connect(options));
}
MySqlPool::Handle MySqlPool::Acquire() {
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(acquireMs_);
    std::unique_lock<std::mutex> lock(mutex_);
    for (;;) {
        if (stopped_) throw std::runtime_error("MySQL pool stopped");
        if (!idle_.empty()) {
            auto connection = std::move(idle_.front()); idle_.pop_front(); ++active_;
            lock.unlock();
            try {
                if (!connection || !connection->isValid()) connection = Connect();
                return Handle(this, std::move(connection));
            } catch (...) {
                lock.lock(); --created_; --active_; ready_.notify_one(); throw;
            }
        }
        if (created_ < maximum_) {
            ++created_; ++active_; lock.unlock();
            try { return Handle(this, Connect()); }
            catch (...) { lock.lock(); --created_; --active_; ready_.notify_one(); throw; }
        }
        if (ready_.wait_until(lock, until) == std::cv_status::timeout)
            throw std::runtime_error("MySQL connection acquire timeout");
    }
}
void MySqlPool::Release(std::unique_ptr<sql::Connection> connection) {
    bool valid = false;
    try { valid = connection && connection->isValid(); } catch (...) {}
    std::lock_guard<std::mutex> lock(mutex_);
    --active_;
    if (valid && !stopped_) idle_.push_back(std::move(connection));
    else --created_;
    ready_.notify_all();
}
void MySqlPool::Stop() {
    std::unique_lock<std::mutex> lock(mutex_);
    stopped_ = true; ready_.notify_all();
    ready_.wait(lock, [this] { return active_ == 0; });
    idle_.clear(); created_ = 0;
}
}
