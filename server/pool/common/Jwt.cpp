#include "Jwt.h"
#include "ConfigMgr.h"
#include "NdtCommon.h"
#include <boost/json.hpp>
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace ndtapp {
namespace {
const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
std::string base64url(const std::string &input) {
    std::string output;
    uint32_t bits = 0; int available = 0;
    for (unsigned char byte : input) {
        bits = (bits << 8) | byte; available += 8;
        while (available >= 6) { available -= 6; output += alphabet[(bits >> available) & 63]; }
    }
    if (available) output += alphabet[(bits << (6 - available)) & 63];
    return output;
}
std::string unbase64url(const std::string &input) {
    if (input.empty() || input.size() % 4 == 1) throw std::runtime_error("invalid base64url length");
    std::string output;
    uint32_t bits = 0; int available = 0;
    for (char c : input) {
        const char *found = std::strchr(alphabet, c);
        if (!found) throw std::runtime_error("invalid base64url character");
        bits = (bits << 6) | uint32_t(found - alphabet); available += 6;
        if (available >= 8) { available -= 8; output += char((bits >> available) & 255); }
    }
    if (available && (bits & ((1u << available) - 1)))
        throw std::runtime_error("invalid base64url padding bits");
    return output;
}
std::string hmac256(const std::string &secret, const std::string &input) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr,
                                   BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0)
        throw std::runtime_error("JWT HMAC provider unavailable");
    DWORD objectSize = 0, actual = 0;
    std::string output(32, '\0');
    try {
        if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
                              sizeof(objectSize), &actual, 0) != 0)
            throw std::runtime_error("JWT HMAC property failed");
        std::vector<unsigned char> object(objectSize);
        if (BCryptCreateHash(alg, &hash, object.data(), objectSize,
                reinterpret_cast<PUCHAR>(const_cast<char *>(secret.data())), ULONG(secret.size()), 0) != 0)
            throw std::runtime_error("JWT HMAC initialization failed");
        if (BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char *>(input.data())),
                           ULONG(input.size()), 0) != 0 ||
            BCryptFinishHash(hash, reinterpret_cast<PUCHAR>(&output[0]), ULONG(output.size()), 0) != 0)
            throw std::runtime_error("JWT HMAC failed");
        BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(alg, 0);
        return output;
    } catch (...) {
        if (hash) BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(alg, 0);
        throw;
    }
}
bool constantEqual(const std::string &a, const std::string &b) {
    if (a.size() != b.size()) return false;
    unsigned char difference = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        difference |= static_cast<unsigned char>(a[i] ^ b[i]);
    return difference == 0;
}
int64_t secondsNow() { return epochMs() / 1000; }
}
Jwt::Jwt() {
    auto &cfg = ConfigMgr::Instance();
    secret_ = cfg.Required("Security", "JwtSecret", "NDT_JWT_SECRET");
    issuer_ = cfg.Get("Security", "JwtIssuer", "LaserUltrasonicServer");
    audience_ = cfg.Get("Security", "JwtAudience", "ndt-acq");
    lifetime_ = cfg.GetInt("Security", "JwtLifetimeSeconds", 300);
    if (secret_.size() < 32 || issuer_.empty() || audience_.empty() ||
        lifetime_ < 1 || lifetime_ > 3600)
        throw std::runtime_error("invalid JWT security configuration");
}
std::string Jwt::Issue(uint64_t userId, const std::string &role,
                       const std::string &nodeId) const {
    return IssueAt(userId, role, nodeId, secondsNow());
}
std::string Jwt::IssueAt(uint64_t userId, const std::string &role,
                         const std::string &nodeId, int64_t issuedAt) const {
    if (!userId || role.empty() || nodeId.empty()) throw std::invalid_argument("invalid JWT claims");
    const boost::json::object header{{"alg","HS256"},{"typ","JWT"}};
    const boost::json::object payload{{"sub",std::to_string(userId)}, {"user_id",userId},
        {"role",role},{"node_id",nodeId},{"iss",issuer_},{"aud",audience_},
        {"iat",issuedAt},{"exp",issuedAt + lifetime_},{"jti",randomHex(12)}};
    const std::string data = base64url(boost::json::serialize(header)) + "." +
                             base64url(boost::json::serialize(payload));
    return data + "." + base64url(hmac256(secret_, data));
}
bool Jwt::Verify(const std::string &token, const std::string &expectedNode,
                 JwtClaims &claims, std::string &error) const {
    claims = {};
    try {
        if (token.size() > 8192 || expectedNode.empty()) throw std::runtime_error("invalid JWT size or node");
        const auto first = token.find('.');
        const auto second = token.find('.', first == std::string::npos ? 0 : first + 1);
        if (first == std::string::npos || second == std::string::npos ||
            token.find('.', second + 1) != std::string::npos)
            throw std::runtime_error("invalid JWT structure");
        const auto header = boost::json::parse(unbase64url(token.substr(0, first))).as_object();
        if (header.at("alg").as_string() != "HS256" || header.at("typ").as_string() != "JWT")
            throw std::runtime_error("unsupported JWT algorithm or type");
        const std::string signedPart = token.substr(0, second);
        if (!constantEqual(hmac256(secret_, signedPart), unbase64url(token.substr(second + 1))))
            throw std::runtime_error("JWT signature mismatch");
        const auto payload = boost::json::parse(unbase64url(token.substr(first + 1, second - first - 1))).as_object();
        if (payload.at("iss").as_string() != issuer_ || payload.at("aud").as_string() != audience_)
            throw std::runtime_error("JWT issuer or audience mismatch");
        claims.nodeId = std::string(payload.at("node_id").as_string());
        claims.role = std::string(payload.at("role").as_string());
        const auto &userId = payload.at("user_id");
        if (userId.is_uint64()) claims.userId = userId.as_uint64();
        else if (userId.is_int64() && userId.as_int64() > 0)
            claims.userId = static_cast<uint64_t>(userId.as_int64());
        else throw std::runtime_error("invalid JWT user ID");
        claims.issuedAt = payload.at("iat").as_int64();
        claims.expiresAt = payload.at("exp").as_int64();
        const std::string subject = std::string(payload.at("sub").as_string());
        const auto now = secondsNow();
        if (!claims.userId || claims.role.empty() || claims.nodeId != expectedNode ||
            subject != std::to_string(claims.userId))
            throw std::runtime_error("JWT user or node mismatch");
        if (claims.issuedAt > now + 60 || claims.expiresAt <= now ||
            claims.expiresAt <= claims.issuedAt ||
            claims.expiresAt - claims.issuedAt > lifetime_)
            throw std::runtime_error("JWT expired or invalid time");
        error.clear(); return true;
    } catch (const std::exception &e) { error = e.what(); claims = {}; return false; }
}
}
