#pragma once
#include "ScanLogic.h"
#include <boost/asio.hpp>
#include <boost/json.hpp>

namespace ndtapp::acq {
class DeviceBridge final {
public:
    DeviceBridge();
    void Configure(const ScanConfig &config);
    boost::json::array Acquire(uint32_t point, double x);
    void Stop();
    static bool Probe();
private:
    boost::json::object Ask(boost::asio::ip::tcp::socket &socket,
                            const boost::json::object &command);
    boost::asio::io_context io_;
    boost::asio::ip::tcp::socket laser_, stage_, daq_;
    boost::asio::streambuf laserBuffer_{16 * 1024 * 1024};
    boost::asio::streambuf stageBuffer_{16 * 1024 * 1024};
    boost::asio::streambuf daqBuffer_{16 * 1024 * 1024};
};
}
