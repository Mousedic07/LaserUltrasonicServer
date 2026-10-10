#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ndtapp::status {
// Transitional compatibility with the original route-token protocol. JWT
// issuance will replace this in the authentication stage.
class SessionManager final {
public:
    std::string Issue(uint64_t userId);
    uint64_t Validate(const std::string &token);
private:
    struct Session { uint64_t userId; int64_t expires; };
    std::mutex mutex_;
    std::unordered_map<std::string, Session> sessions_;
};
}
