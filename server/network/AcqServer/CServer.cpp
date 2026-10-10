#include "CServer.h"
#include "server/logic/AcqServer/LogicSystem.h"
#include <algorithm>
#include <iostream>

namespace ndtapp::acq {
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
CServer::CServer(asio::io_context &acceptIo, AsioIOServicePool &ioPool,
                 LogicSystem &logic, const std::string &host, unsigned short port,
                 std::size_t maximum)
    : acceptor_(acceptIo), ioPool_(ioPool), logic_(logic), maximum_(maximum) {
    const tcp::endpoint endpoint(asio::ip::make_address(host), port);
    acceptor_.open(endpoint.protocol()); acceptor_.set_option(tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint); acceptor_.listen();
}
void CServer::Start() { Accept(); }
void CServer::Stop() {
    boost::system::error_code ignored; acceptor_.close(ignored);
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    for (auto &weak : sessions_) if (auto session = weak.lock()) session->Close();
}
void CServer::Accept() {
    auto socket = std::make_shared<tcp::socket>(ioPool_.GetIOService());
    acceptor_.async_accept(*socket, [this, socket](boost::system::error_code error) {
        if (!acceptor_.is_open()) return;
        if (!error) {
            if (active_.fetch_add(1) < maximum_) {
                logic_.OnConnect();
                auto session = std::make_shared<CSession>(std::move(*socket), logic_, [this] {
                    --active_; logic_.OnDisconnect();
                });
                {
                    std::lock_guard<std::mutex> lock(sessionsMutex_);
                    sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(),
                        [](const auto &weak) { return weak.expired(); }), sessions_.end());
                    sessions_.push_back(session);
                }
                session->Start();
            } else { --active_; boost::system::error_code ignored; socket->close(ignored); }
        } else std::cerr << "Acq accept: " << error.message() << '\n';
        Accept();
    });
}
}
