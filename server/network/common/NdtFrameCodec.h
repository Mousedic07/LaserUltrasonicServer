#pragma once
#include <boost/json.hpp>
#include <array>
#include <cstdint>
#include <string>

namespace ndtapp {
class NdtFrameCodec final {
public:
    static constexpr std::size_t HeaderSize = 12;
    static constexpr uint32_t MaxBody = 1048576;
    struct Header { uint16_t type; uint32_t length; };
    static Header ParseHeader(const std::array<char, HeaderSize> &bytes);
    static boost::json::value ParseBody(const std::string &body);
    static std::string Encode(uint16_t type, const boost::json::value &body);
};
}
