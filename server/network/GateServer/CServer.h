#pragma once
#include "HttpConnection.h"
#include <boost/asio.hpp>
#include <atomic>
#include <cstddef>
#include <memory>

namespace ndtapp::gate {
class CServer final {
public:
    CServer(boost::asio::io_context &io, LogicSystem &logic,
            const std::string &host, unsigned short port, std::size_t maximum);
    void Start();
    void Stop();
private:
    void Accept();
    boost::asio::ip::tcp::acceptor acceptor_;
    LogicSystem &logic_;
    std::shared_ptr<std::atomic<std::size_t>> active_;
    std::size_t maximum_;
};
}
