#pragma once
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace ndtapp {
using tcp = boost::asio::ip::tcp;
using Json = boost::json::value;

std::string env(const char *key, const char *fallback);
int envInt(const char *key, int fallback);
std::string address(const char *hostKey, const char *portKey, int fallbackPort);
void deadline(grpc::ClientContext &context, int timeoutMs = 3000);
std::string randomHex(std::size_t bytes = 16);
std::string sha256Hex(const std::string &data);
std::string sha256File(const std::string &path);
std::string jsonString(const Json &value);
Json parseJson(const std::string &line);
std::string readLine(tcp::socket &socket, std::size_t limit = 16 * 1024 * 1024);
void writeLine(tcp::socket &socket, const Json &value);
void sendFrame(tcp::socket &socket, uint16_t type, const Json &value);
bool readFrame(tcp::socket &socket, uint16_t &type, Json &value);
int64_t epochMs();

template <typename T> class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {}
    bool push(T value) {
        std::unique_lock<std::mutex> lock(mutex_);
        nonfull_.wait(lock, [this] { return closed_ || values_.size() < capacity_; });
        if (closed_) return false;
        values_.push_back(std::move(value));
        nonempty_.notify_one();
        return true;
    }
    bool pop(T &value) {
        std::unique_lock<std::mutex> lock(mutex_);
        nonempty_.wait(lock, [this] { return closed_ || !values_.empty(); });
        if (values_.empty()) return false;
        value = std::move(values_.front());
        values_.pop_front();
        nonfull_.notify_one();
        return true;
    }
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true; nonempty_.notify_all(); nonfull_.notify_all();
    }
private:
    std::size_t capacity_;
    std::deque<T> values_;
    std::mutex mutex_;
    std::condition_variable nonempty_, nonfull_;
    bool closed_ = false;
};
}
