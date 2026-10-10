#include "NdtCommon.h"
#include <cassert>
#include <iostream>
#include <thread>

int main() {
    boost::asio::io_context io;
    ndtapp::tcp::acceptor acceptor(io, {boost::asio::ip::address_v4::loopback(), 0});
    const auto port = acceptor.local_endpoint().port();
    std::thread server([&] {
        ndtapp::tcp::socket socket(io); acceptor.accept(socket);
        uint16_t type = 0; ndtapp::Json value;
        assert(ndtapp::readFrame(socket, type, value) && type == 1);
        assert(value.as_object().at("token").as_string() == "split");
        assert(ndtapp::readFrame(socket, type, value) && type == 3);
    });
    ndtapp::tcp::socket client(io);
    client.connect({boost::asio::ip::address_v4::loopback(), port});
    // Explicit length avoids C string truncation at the zero bytes.
    std::string packet("NDT1", 4);
    packet.append("\x00\x01\x00\x01\x00\x00\x00\x11", 8);
    packet += "{\"token\":\"split\"}";
    const std::string stop = std::string("NDT1", 4) + std::string("\x00\x01\x00\x03\x00\x00\x00\x02", 8) + "{}";
    boost::asio::write(client, boost::asio::buffer(packet.data(), 3));
    boost::asio::write(client, boost::asio::buffer(packet.data() + 3, 5));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    boost::asio::write(client, boost::asio::buffer(packet.data() + 8, packet.size() - 8));
    boost::asio::write(client, boost::asio::buffer(stop));
    server.join();
    std::cout << "fragmented and coalesced TCP frames passed\n";
}
