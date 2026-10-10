#include "DeviceBridge.h"
#include "ConfigMgr.h"
#include "NdtCommon.h"
#include <winsock2.h>
#include <istream>

namespace ndtapp::acq {
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace {
void setTimeout(tcp::socket &socket, int ms) {
    const DWORD timeout = DWORD(ms);
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char *>(&timeout), sizeof(timeout));
    setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char *>(&timeout), sizeof(timeout));
}
}
DeviceBridge::DeviceBridge() : laser_(io_), stage_(io_), daq_(io_) {
    auto &cfg = ConfigMgr::Instance();
    const int base = cfg.GetInt("Device", "BasePort", 9000, "NDT_DEVICE_BASE_PORT");
    const auto host = asio::ip::make_address(cfg.Get("Device", "Host", "127.0.0.1", "NDT_DEVICE_HOST"));
    const int timeout = cfg.GetInt("Timeout", "DeviceTimeoutMs", 5000);
    laser_.connect({host, uint16_t(base)}); setTimeout(laser_, timeout);
    stage_.connect({host, uint16_t(base + 1)}); setTimeout(stage_, timeout);
    daq_.connect({host, uint16_t(base + 2)}); setTimeout(daq_, timeout);
}
boost::json::object DeviceBridge::Ask(tcp::socket &socket, const boost::json::object &command) {
    writeLine(socket, command);
    auto &buffer = (&socket == &laser_) ? laserBuffer_ :
        ((&socket == &stage_) ? stageBuffer_ : daqBuffer_);
    asio::read_until(socket, buffer, '\n');
    std::istream input(&buffer);
    std::string line; std::getline(input, line);
    auto reply = parseJson(line).as_object();
    if (!reply.at("ok").as_bool())
        throw std::runtime_error(std::string(reply.at("error").as_string()));
    return reply;
}
void DeviceBridge::Configure(const ScanConfig &config) {
    Ask(laser_, {{"command","connect"}});
    Ask(laser_, {{"command","set_frequency"},{"frequency_hz",config.excitation}});
    Ask(stage_, {{"command","set_step"},{"step_mm",config.step}});
    Ask(daq_, {{"command","configure"},{"sample_rate_hz",config.rate},
        {"sample_count",config.samples},{"channel_count",config.channels},
        {"excitation_frequency_hz",config.excitation}});
    Ask(daq_, {{"command","start_acquisition"}});
}
boost::json::array DeviceBridge::Acquire(uint32_t point, double x) {
    Ask(stage_, {{"command","move_to"},{"x_mm",x},{"y_mm",0}});
    Ask(laser_, {{"command","trigger"}});
    return Ask(daq_, {{"command","read_waveform"},{"point",point},{"x_mm",x},{"y_mm",0}})
        .at("frames").as_array();
}
void DeviceBridge::Stop() {
    try { Ask(daq_, {{"command","stop_acquisition"}}); } catch (...) {}
    try { Ask(laser_, {{"command","stop"}}); } catch (...) {}
}
bool DeviceBridge::Probe() {
    try { DeviceBridge bridge; return true; } catch (...) { return false; }
}
}
