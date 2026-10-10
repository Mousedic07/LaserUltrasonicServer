#include "CSession.h"
#include "server/logic/AcqServer/LogicSystem.h"
#include <iostream>

namespace ndtapp::acq {
namespace asio = boost::asio;
CSession::CSession(asio::ip::tcp::socket socket, LogicSystem &logic,
                   std::function<void()> onClosed)
    : socket_(std::move(socket)), logic_(logic), onClosed_(std::move(onClosed)) {}
void CSession::Start() {
    auto self = shared_from_this();
    asio::post(socket_.get_executor(), [self] { self->ReadHeader(); });
}
void CSession::ResumeRead() {
    auto self = shared_from_this();
    asio::post(socket_.get_executor(), [self] { if (!self->closed_) self->ReadHeader(); });
}
void CSession::ReadHeader() {
    auto self = shared_from_this();
    asio::async_read(socket_, asio::buffer(header_), [self](boost::system::error_code error, std::size_t) {
        if (error) { self->Close(); return; }
        try { self->ReadBody(NdtFrameCodec::ParseHeader(self->header_)); }
        catch (const std::exception &e) {
            std::cerr << "Acq frame header: " << e.what() << '\n'; self->Close();
        }
    });
}
void CSession::ReadBody(NdtFrameCodec::Header header) {
    body_.assign(header.length, '\0');
    auto self = shared_from_this();
    asio::async_read(socket_, asio::buffer(body_), [self, header](boost::system::error_code error, std::size_t) {
        if (error) { self->Close(); return; }
        try {
            MsgNode node{header.type, NdtFrameCodec::ParseBody(self->body_), self};
            if (!self->logic_.PostMsgToQue(std::move(node))) {
                self->Send(8, {{"error","business queue full"}}); self->Close();
            }
        } catch (const std::exception &e) {
            std::cerr << "Acq frame body: " << e.what() << '\n'; self->Close();
        }
    });
}
bool CSession::Send(uint16_t type, const boost::json::value &body) {
    if (closed_) return false;
    std::shared_ptr<std::string> packet;
    try { packet = std::make_shared<std::string>(NdtFrameCodec::Encode(type, body)); }
    catch (const std::exception &) { RequestStop(); return false; }
    if (queued_.fetch_add(1) >= 64) {
        --queued_; RequestStop(); Close(); return false;
    }
    auto self = shared_from_this();
    asio::post(socket_.get_executor(), [self, packet] {
        if (self->closed_) { --self->queued_; return; }
        const bool idle = self->writes_.empty();
        self->writes_.push_back(packet);
        if (idle) self->WriteNext();
    });
    return true;
}
void CSession::WriteNext() {
    if (writes_.empty() || closed_) return;
    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(*writes_.front()), [self](boost::system::error_code error, std::size_t) {
        --self->queued_;
        self->writes_.pop_front();
        if (error) self->Close();
        else self->WriteNext();
    });
}
void CSession::Close() {
    if (closed_.exchange(true)) return;
    stop_ = true;
    onClosed_();
    auto self = shared_from_this();
    asio::post(socket_.get_executor(), [self] {
        boost::system::error_code ignored;
        self->socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
        self->socket_.close(ignored);
    });
}
}
