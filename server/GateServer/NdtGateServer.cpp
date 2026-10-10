#include "NdtCommon.h"
#include "ndt.grpc.pb.h"
#include <boost/beast.hpp>
#include <atomic>
#include <iostream>
#include <thread>

namespace {
namespace beast = boost::beast;
namespace http = beast::http;
std::atomic<int> active{0};

void handle(ndtapp::tcp::socket socket, ndt::AuthService::Stub *auth,
            ndt::StatusService::Stub *status) {
    try {
        beast::tcp_stream stream(std::move(socket));
        stream.expires_after(std::chrono::seconds(6));
        beast::flat_buffer buffer;
        http::request_parser<http::string_body> parser;
        parser.body_limit(4096);
        http::read(stream, buffer, parser);
        auto request = parser.release();
        http::response<http::string_body> response{http::status::ok, request.version()};
        response.set(http::field::content_type, "application/json");
        response.keep_alive(false);
        boost::json::object result;
        if (request.method() != http::verb::post || request.target() != "/login") {
            response.result(http::status::not_found);
            result["ok"] = false; result["error"] = "route not found";
        } else {
            try {
                auto object = ndtapp::parseJson(request.body()).as_object();
                const std::string user = std::string(object.at("username").as_string());
                const std::string password = std::string(object.at("password").as_string());
                ndt::LoginRequest login;
                login.set_username(user); login.set_password(password);
                ndt::LoginReply loginReply;
                grpc::ClientContext authContext;
                ndtapp::deadline(authContext);
                auto authStatus = auth->Verify(&authContext, login, &loginReply);
                if (!authStatus.ok()) {
                    response.result(http::status::service_unavailable);
                    result["ok"] = false; result["error"] = "AuthServer unavailable";
                } else if (!loginReply.ok()) {
                    response.result(http::status::unauthorized);
                    result["ok"] = false; result["error"] = loginReply.error();
                } else {
                    ndt::RouteRequest route;
                    route.set_user_id(loginReply.user_id());
                    ndt::RouteReply routeReply;
                    grpc::ClientContext routeContext;
                    ndtapp::deadline(routeContext);
                    auto routeStatus = status->Route(&routeContext, route, &routeReply);
                    if (!routeStatus.ok() || !routeReply.ok()) {
                        response.result(http::status::service_unavailable);
                        result["ok"] = false;
                        result["error"] = routeStatus.ok() ? routeReply.error() : "StatusServer unavailable";
                    } else {
                        result["ok"] = true; result["user_id"] = loginReply.user_id();
                        result["role"] = loginReply.role(); result["host"] = routeReply.host();
                        result["port"] = routeReply.port(); result["token"] = routeReply.token();
                        result["node_id"] = routeReply.node_id();
                    }
                }
            } catch (const std::exception &e) {
                response.result(http::status::bad_request);
                result["ok"] = false; result["error"] = "invalid JSON login request";
            }
        }
        response.body() = boost::json::serialize(result);
        response.prepare_payload();
        http::write(stream, response);
        beast::error_code ignored;
        stream.socket().shutdown(ndtapp::tcp::socket::shutdown_send, ignored);
    } catch (const std::exception &e) {
        std::cerr << "Gate connection error: " << e.what() << '\n';
    }
    --active;
}
}

int main() {
    const std::string authAddress = ndtapp::address("NDT_AUTH_HOST", "NDT_AUTH_PORT", 50051);
    const std::string statusAddress = ndtapp::address("NDT_STATUS_HOST", "NDT_STATUS_PORT", 50052);
    auto auth = ndt::AuthService::NewStub(grpc::CreateChannel(authAddress, grpc::InsecureChannelCredentials()));
    auto status = ndt::StatusService::NewStub(grpc::CreateChannel(statusAddress, grpc::InsecureChannelCredentials()));
    const int port = ndtapp::envInt("NDT_GATE_PORT", 8080);
    boost::asio::io_context io;
    ndtapp::tcp::acceptor acceptor(io, {boost::asio::ip::make_address(ndtapp::env("NDT_GATE_HOST", "127.0.0.1")), uint16_t(port)});
    std::cout << "GateServer listening on " << port << std::endl;
    for (;;) {
        ndtapp::tcp::socket socket(io);
        acceptor.accept(socket);
        if (active.load() >= 64) { socket.close(); continue; }
        ++active;
        std::thread(handle, std::move(socket), auth.get(), status.get()).detach();
    }
}
