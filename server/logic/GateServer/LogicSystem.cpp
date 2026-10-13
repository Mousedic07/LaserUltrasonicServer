#include "LogicSystem.h"
#include "ConfigMgr.h"
#include "NdtCommon.h"
#include <iostream>
#include <stdexcept>

namespace ndtapp::gate {
namespace http = boost::beast::http;
LogicSystem::LogicSystem()
    : capacity_(std::size_t(ConfigMgr::Instance().GetInt("Queue", "LogicCapacity", 128))) {
    if (!capacity_ || capacity_ > 65536) throw std::runtime_error("invalid Gate logic capacity");
    RegisterCallBacks();
    worker_ = std::thread([this] { DealMsg(); });
}
LogicSystem::~LogicSystem() { Stop(); }
bool LogicSystem::PostMsgToQue(http::verb method, std::string target,
                                std::string body, std::function<void(HttpResult)> complete) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_ || messages_.size() >= capacity_) return false;
    messages_.push_back({method, std::move(target), std::move(body), std::move(complete)});
    queued_.notify_one();
    return true;
}
void LogicSystem::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return;
        stopping_ = true;
        messages_.clear();
    }
    queued_.notify_all();
    if (worker_.joinable()) worker_.join();
}
void LogicSystem::DealMsg() {
    for (;;) {
        Request request;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queued_.wait(lock, [this] { return stopping_ || !messages_.empty(); });
            if (stopping_) return;
            request = std::move(messages_.front());
            messages_.pop_front();
        }
        try {
            request.complete(Handle(request.method, request.target, request.body));
        } catch (const std::exception &error) {
            std::cerr << "Gate logic completion: " << error.what() << '\n';
        }
    }
}
void LogicSystem::RegisterCallBacks() {
    callbacks_["/login"] = [this](const std::string &body) { return Login(body); };
}
HttpResult LogicSystem::Handle(http::verb method, const std::string &target,
                               const std::string &body) {
    const auto found = callbacks_.find(target);
    if (method != http::verb::post || found == callbacks_.end())
        return {http::status::not_found, {{"ok",false},{"error","route not found"}}};
    try { return found->second(body); }
    catch (const std::exception &) {
        return {http::status::bad_request, {{"ok",false},{"error","invalid JSON login request"}}};
    }
}
HttpResult LogicSystem::Login(const std::string &body) {
    const auto object = parseJson(body).as_object();
    const std::string user = std::string(object.at("username").as_string());
    const std::string password = std::string(object.at("password").as_string());
    ndt::LoginReply login;
    const auto verified = auth_.Verify(user, password, login);
    if (!verified.ok())
        return {http::status::service_unavailable, {{"ok",false},{"error","AuthServer unavailable"}}};
    if (!login.ok())
        return {http::status::unauthorized, {{"ok",false},{"error",login.error()}}};
    ndt::RouteReply route;
    const auto routed = status_.Route(login.user_id(), route);
    if (!routed.ok() || !route.ok())
        return {http::status::service_unavailable, {{"ok",false},
            {"error",routed.ok() ? route.error() : "StatusServer unavailable"}}};
    return {http::status::ok, {{"ok",true},{"user_id",login.user_id()},
        {"role",login.role()},{"host",route.host()},{"port",route.port()},
        {"token",jwt_.Issue(login.user_id(), login.role(), route.node_id())},
        {"node_id",route.node_id()}}};
}
}
