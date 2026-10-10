#include "NdtFrameCodec.h"
#include <stdexcept>

namespace ndtapp {
namespace {
uint16_t read16(const char *p) { return (uint16_t(uint8_t(p[0])) << 8) | uint8_t(p[1]); }
uint32_t read32(const char *p) { return (uint32_t(uint8_t(p[0])) << 24) |
    (uint32_t(uint8_t(p[1])) << 16) | (uint32_t(uint8_t(p[2])) << 8) | uint8_t(p[3]); }
}
NdtFrameCodec::Header NdtFrameCodec::ParseHeader(const std::array<char, HeaderSize> &bytes) {
    if (std::string(bytes.data(), 4) != "NDT1" || read16(bytes.data() + 4) != 1)
        throw std::runtime_error("invalid NDT frame header");
    const auto length = read32(bytes.data() + 8);
    if (length > MaxBody) throw std::runtime_error("NDT frame exceeds 1 MiB");
    return {read16(bytes.data() + 6), length};
}
boost::json::value NdtFrameCodec::ParseBody(const std::string &body) {
    auto value = boost::json::parse(body);
    if (!value.is_object()) throw std::runtime_error("NDT frame body must be a JSON object");
    return value;
}
std::string NdtFrameCodec::Encode(uint16_t type, const boost::json::value &body) {
    const std::string data = boost::json::serialize(body);
    if (data.size() > MaxBody) throw std::runtime_error("NDT frame exceeds 1 MiB");
    std::string packet{"NDT1", 4};
    packet.push_back(0); packet.push_back(1);
    packet.push_back(char(type >> 8)); packet.push_back(char(type));
    const auto size = uint32_t(data.size());
    packet.push_back(char(size >> 24)); packet.push_back(char(size >> 16));
    packet.push_back(char(size >> 8)); packet.push_back(char(size));
    packet += data;
    return packet;
}
}
