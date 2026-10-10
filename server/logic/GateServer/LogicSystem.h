#pragma once
#include "server/network/GateServer/AuthGrpcClient.h"
#include "server/network/GateServer/StatusGrpcClient.h"
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <functional>
#include <map>
#include <string>

namespace ndtapp::gate {
struct HttpResult {
    boost::beast::http::status status = boost::beast::http::status::ok;
    boost::json::object body;
};
class LogicSystem final {
public:
    LogicSystem();
    HttpResult Handle(boost::beast::http::verb method, const std::string &target,
                      const std::string &body);
private:
    using Callback = std::function<HttpResult(const std::string &)>;
    void RegisterCallBacks();
    HttpResult Login(const std::string &body);
    std::map<std::string, Callback> callbacks_;
    AuthGrpcClient auth_;
    StatusGrpcClient status_;
};
}
