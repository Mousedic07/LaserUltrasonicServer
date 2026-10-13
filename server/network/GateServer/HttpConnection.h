#pragma once
#include "server/logic/GateServer/LogicSystem.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <functional>
#include <memory>

namespace ndtapp::gate {
class HttpConnection final : public std::enable_shared_from_this<HttpConnection> {
public:
    HttpConnection(boost::asio::ip::tcp::socket socket, LogicSystem &logic,
                   std::function<void()> onDone);
    ~HttpConnection();
    void Start();
private:
    void Respond(HttpResult result, unsigned version);
    boost::beast::tcp_stream stream_;
    boost::beast::flat_buffer buffer_;
    boost::beast::http::request_parser<boost::beast::http::string_body> parser_;
    std::shared_ptr<boost::beast::http::response<boost::beast::http::string_body>> response_;
    LogicSystem &logic_;
    std::function<void()> onDone_;
};
}
