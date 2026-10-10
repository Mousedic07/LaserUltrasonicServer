#include <boost/json/src.hpp>
#include "NdtCommon.h"
#include <bcrypt.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cstdlib>

namespace ndtapp {
std::string env(const char *key, const char *fallback) {
    const char *found = std::getenv(key);
    return found && *found ? found : fallback;
}
int envInt(const char *key, int fallback) {
    const char *found = std::getenv(key);
    if (!found || !*found) return fallback;
    try { return std::stoi(found); } catch (...) { return fallback; }
}
std::string address(const char *hostKey, const char *portKey, int fallbackPort) {
    return env(hostKey, "127.0.0.1") + ":" + std::to_string(envInt(portKey, fallbackPort));
}
void deadline(grpc::ClientContext &context, int timeoutMs) {
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(timeoutMs));
}
std::string randomHex(std::size_t bytes) {
    std::vector<unsigned char> data(bytes);
    if (BCryptGenRandom(nullptr, data.data(), ULONG(data.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        throw std::runtime_error("BCryptGenRandom failed");
    std::ostringstream out;
    for (auto b : data) out << std::hex << std::setw(2) << std::setfill('0') << int(b);
    return out.str();
}
std::string sha256Hex(const std::string &data) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectSize = 0, actual = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        throw std::runtime_error("SHA-256 provider failed");
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &actual, 0);
    std::vector<unsigned char> object(objectSize), digest(32);
    if (BCryptCreateHash(alg, &hash, object.data(), objectSize, nullptr, 0, 0) != 0)
        throw std::runtime_error("SHA-256 hash failed");
    BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char *>(data.data())), ULONG(data.size()), 0);
    BCryptFinishHash(hash, digest.data(), ULONG(digest.size()), 0);
    BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(alg, 0);
    std::ostringstream out;
    for (auto b : digest) out << std::hex << std::setw(2) << std::setfill('0') << int(b);
    return out.str();
}
std::string sha256File(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file for SHA-256");
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectSize = 0, actual = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        throw std::runtime_error("SHA-256 provider failed");
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &actual, 0);
    std::vector<unsigned char> object(objectSize), digest(32);
    if (BCryptCreateHash(alg, &hash, object.data(), objectSize, nullptr, 0, 0) != 0)
        throw std::runtime_error("SHA-256 hash failed");
    std::array<char, 65536> block{};
    while (in) {
        in.read(block.data(), block.size());
        const auto count = in.gcount();
        if (count > 0) BCryptHashData(hash, reinterpret_cast<PUCHAR>(block.data()), ULONG(count), 0);
    }
    BCryptFinishHash(hash, digest.data(), ULONG(digest.size()), 0);
    BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(alg, 0);
    std::ostringstream out;
    for (auto b : digest) out << std::hex << std::setw(2) << std::setfill('0') << int(b);
    return out.str();
}
std::string jsonString(const Json &value) { return boost::json::serialize(value); }
Json parseJson(const std::string &line) { return boost::json::parse(line); }
std::string readLine(tcp::socket &socket, std::size_t limit) {
    std::string line;
    for (;;) {
        char c;
        boost::asio::read(socket, boost::asio::buffer(&c, 1));
        if (c == '\n') return line;
        line.push_back(c);
        if (line.size() > limit) throw std::runtime_error("Line too long");
    }
}
void writeLine(tcp::socket &socket, const Json &value) {
    const std::string line = jsonString(value) + "\n";
    boost::asio::write(socket, boost::asio::buffer(line));
}
static uint16_t be16(const char *p) { return (uint16_t(uint8_t(p[0])) << 8) | uint8_t(p[1]); }
static uint32_t be32(const char *p) { return (uint32_t(uint8_t(p[0])) << 24) |
    (uint32_t(uint8_t(p[1])) << 16) | (uint32_t(uint8_t(p[2])) << 8) | uint8_t(p[3]); }
void sendFrame(tcp::socket &socket, uint16_t type, const Json &value) {
    const std::string body = jsonString(value);
    if (body.size() > 1048576) throw std::runtime_error("Frame too large");
    char header[12] = {'N','D','T','1',0,1,char(type >> 8),char(type),
                       char(body.size() >> 24),char(body.size() >> 16),char(body.size() >> 8),char(body.size())};
    std::array<boost::asio::const_buffer,2> buffers{boost::asio::buffer(header),boost::asio::buffer(body)};
    boost::asio::write(socket, buffers);
}
bool readFrame(tcp::socket &socket, uint16_t &type, Json &value) {
    char header[12];
    boost::system::error_code error;
    std::size_t count = boost::asio::read(socket, boost::asio::buffer(header), error);
    if (error == boost::asio::error::eof && count == 0) return false;
    if (error) throw boost::system::system_error(error);
    if (std::string(header, 4) != "NDT1" || be16(header + 4) != 1)
        throw std::runtime_error("Invalid NDT frame header");
    type = be16(header + 6);
    uint32_t length = be32(header + 8);
    if (length > 1048576) throw std::runtime_error("Frame exceeds 1 MiB");
    std::string body(length, '\0');
    boost::asio::read(socket, boost::asio::buffer(body));
    value = parseJson(body);
    return true;
}
int64_t epochMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
}
