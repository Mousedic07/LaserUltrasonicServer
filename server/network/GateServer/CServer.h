#pragma once
#include "HttpConnection.h"
#include <boost/asio.hpp>
#include <atomic>
#include <cstddef>

namespace ndtapp::gate {
class CServer final {
public:
    CServer(boost::asio::io_context &io, LogicSystem &logic,
            LogicThreadPool &business, const std::string &host, unsigned short port,
            std::size_t maximum);
    void Start();
    void Stop();
private:
    void Accept();
    boost::asio::ip::tcp::acceptor acceptor_;
    LogicSystem &logic_;
    LogicThreadPool &business_;
    std::atomic<std::size_t> active_{0};
    std::size_t maximum_;
};
}
