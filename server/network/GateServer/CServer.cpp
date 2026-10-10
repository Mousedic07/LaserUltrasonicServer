#include "CServer.h"
#include <iostream>

namespace ndtapp::gate {
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
CServer::CServer(asio::io_context &io, LogicSystem &logic, LogicThreadPool &business,
                 const std::string &host, unsigned short port, std::size_t maximum)
    : acceptor_(io), logic_(logic), business_(business), maximum_(maximum) {
    const tcp::endpoint endpoint(asio::ip::make_address(host), port);
    acceptor_.open(endpoint.protocol()); acceptor_.set_option(tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint); acceptor_.listen();
}
void CServer::Start() { Accept(); }
void CServer::Stop() { boost::system::error_code ignored; acceptor_.close(ignored); }
void CServer::Accept() {
    acceptor_.async_accept([this](boost::system::error_code error, tcp::socket socket) {
        if (!acceptor_.is_open()) return;
        if (!error) {
            if (active_.fetch_add(1) < maximum_)
                std::make_shared<HttpConnection>(std::move(socket), logic_, business_,
                    [this] { --active_; })->Start();
            else { --active_; boost::system::error_code ignored; socket.close(ignored); }
        } else std::cerr << "Gate accept: " << error.message() << '\n';
        Accept();
    });
}
}
