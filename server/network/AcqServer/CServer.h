#pragma once
#include "CSession.h"
#include "AsioIOServicePool.h"
#include <boost/asio.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace ndtapp::acq {
class CServer final {
public:
    CServer(boost::asio::io_context &acceptIo, AsioIOServicePool &ioPool,
            LogicSystem &logic, const std::string &host, unsigned short port,
            std::size_t maximum);
    void Start();
    void Stop();
private:
    void Accept();
    boost::asio::ip::tcp::acceptor acceptor_;
    AsioIOServicePool &ioPool_;
    LogicSystem &logic_;
    std::atomic<std::size_t> active_{0};
    std::size_t maximum_;
    std::mutex sessionsMutex_;
    std::vector<std::weak_ptr<CSession>> sessions_;
};
}
