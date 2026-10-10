#pragma once
#include "NdtFrameCodec.h"
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <array>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <string>

namespace ndtapp::acq {
class LogicSystem;
class CSession final : public std::enable_shared_from_this<CSession> {
public:
    CSession(boost::asio::ip::tcp::socket socket, LogicSystem &logic,
             std::function<void()> onClosed);
    void Start();
    void ResumeRead();
    bool Send(uint16_t type, const boost::json::value &body);
    void Close();
    bool Authenticated() const { return authenticated_.load(); }
    void SetAuthenticated() { authenticated_ = true; }
    bool StopRequested() const { return stop_.load(); }
    void RequestStop() { stop_ = true; }
    void ResetStop() { stop_ = false; }
    bool BeginScan() { return !scanRunning_.exchange(true); }
    void EndScan() { scanRunning_ = false; }
private:
    void ReadHeader();
    void ReadBody(NdtFrameCodec::Header header);
    void WriteNext();
    boost::asio::ip::tcp::socket socket_;
    LogicSystem &logic_;
    std::function<void()> onClosed_;
    std::array<char, NdtFrameCodec::HeaderSize> header_{};
    std::string body_;
    std::deque<std::shared_ptr<std::string>> writes_;
    std::atomic<std::size_t> queued_{0};
    std::atomic<bool> closed_{false}, authenticated_{false}, stop_{false}, scanRunning_{false};
};
}
