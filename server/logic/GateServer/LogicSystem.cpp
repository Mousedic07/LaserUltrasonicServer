#include "LogicSystem.h"
#include "NdtCommon.h"

namespace ndtapp::gate {
namespace http = boost::beast::http;
LogicSystem::LogicSystem() { RegisterCallBacks(); }
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
