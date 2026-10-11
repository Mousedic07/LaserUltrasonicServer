#pragma once
#include <cstdint>
#include <string>

namespace ndtapp {
struct JwtClaims {
    uint64_t userId = 0;
    std::string role, nodeId;
    int64_t issuedAt = 0, expiresAt = 0;
};
// HS256 trust boundary: only GateServer signs; each AcqServer verifies with
// the same environment-supplied secret. StatusServer never stores JWTs.
class Jwt final {
public:
    Jwt();
    std::string Issue(uint64_t userId, const std::string &role,
                      const std::string &nodeId) const;
    std::string IssueAt(uint64_t userId, const std::string &role,
                        const std::string &nodeId, int64_t issuedAt) const;
    bool Verify(const std::string &token, const std::string &expectedNode,
                JwtClaims &claims, std::string &error) const;
private:
    std::string secret_, issuer_, audience_;
    int64_t lifetime_ = 300;
};
}
