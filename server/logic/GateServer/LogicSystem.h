#pragma once
#include "server/network/GateServer/AuthGrpcClient.h"
#include "server/network/GateServer/StatusGrpcClient.h"
#include "Jwt.h"
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace ndtapp::gate {
struct HttpResult {
    boost::beast::http::status status = boost::beast::http::status::ok;
    boost::json::object body;
};
class LogicSystem final {
public:
    LogicSystem();
    ~LogicSystem();
    LogicSystem(const LogicSystem &) = delete;
    LogicSystem &operator=(const LogicSystem &) = delete;
    bool PostMsgToQue(boost::beast::http::verb method, std::string target,
                      std::string body, std::function<void(HttpResult)> complete);
    void Stop();
private:
    struct Request {
        boost::beast::http::verb method;
        std::string target;
        std::string body;
        std::function<void(HttpResult)> complete;
    };
    using Callback = std::function<HttpResult(const std::string &)>;
    void RegisterCallBacks();
    void DealMsg();
    HttpResult Handle(boost::beast::http::verb method, const std::string &target,
                      const std::string &body);
    HttpResult Login(const std::string &body);
    std::map<std::string, Callback> callbacks_;
    std::mutex mutex_;
    std::condition_variable queued_;
    std::deque<Request> messages_;
    std::size_t capacity_;
    bool stopping_ = false;
    std::thread worker_;
    AuthGrpcClient auth_;
    StatusGrpcClient status_;
    Jwt jwt_;
};
}
