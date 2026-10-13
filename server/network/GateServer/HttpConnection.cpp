#include "HttpConnection.h"
#include <iostream>

namespace ndtapp::gate {
namespace http = boost::beast::http;
HttpConnection::HttpConnection(boost::asio::ip::tcp::socket socket, LogicSystem &logic,
                               std::function<void()> onDone)
    : stream_(std::move(socket)), logic_(logic), onDone_(std::move(onDone)) {
    parser_.body_limit(4096);
}
HttpConnection::~HttpConnection() { onDone_(); }
void HttpConnection::Start() {
    auto self = shared_from_this();
    stream_.expires_after(std::chrono::seconds(6));
    http::async_read(stream_, buffer_, parser_, [self](boost::beast::error_code error, std::size_t) {
        if (error) { std::cerr << "Gate HTTP read: " << error.message() << '\n'; return; }
        auto request = self->parser_.release();
        const unsigned version = request.version();
        const auto method = request.method();
        const std::string target(request.target());
        std::string body = std::move(request.body());
        if (!self->logic_.PostMsgToQue(method, target, std::move(body), [self, version](HttpResult result) {
            boost::asio::post(self->stream_.get_executor(),
                [self, result=std::move(result), version]() mutable {
                    self->Respond(std::move(result), version);
                });
        })) {
            self->Respond({http::status::service_unavailable,
                          {{"ok",false},{"error","server busy"}}}, version);
        }
    });
}
void HttpConnection::Respond(HttpResult result, unsigned version) {
    response_ = std::make_shared<http::response<http::string_body>>(result.status, version);
    response_->set(http::field::content_type, "application/json");
    response_->keep_alive(false);
    response_->body() = boost::json::serialize(result.body);
    response_->prepare_payload();
    auto self = shared_from_this();
    stream_.expires_after(std::chrono::seconds(6));
    http::async_write(stream_, *response_, [self](boost::beast::error_code error, std::size_t) {
        if (error) std::cerr << "Gate HTTP write: " << error.message() << '\n';
        boost::beast::error_code ignored;
        self->stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ignored);
    });
}
}
