#include "ConfigMgr.h"
#include "Jwt.h"
#include "NdtCommon.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv) {
    try {
        _putenv_s("NDT_JWT_SECRET", ndtapp::randomHex(32).c_str());
        ndtapp::ConfigMgr::Instance().Load("GateServer", argc, argv);
        ndtapp::Jwt jwt;
        const auto token = jwt.Issue(123, "operator", "acq1");
        ndtapp::JwtClaims claims; std::string error;
        if (!jwt.Verify(token, "acq1", claims, error) || claims.userId != 123 ||
            claims.role != "operator" || claims.nodeId != "acq1")
            throw std::runtime_error("valid JWT rejected: " + error);
        if (jwt.Verify(token, "acq2", claims, error))
            throw std::runtime_error("wrong node JWT accepted");
        const auto expired = jwt.IssueAt(123, "operator", "acq1", ndtapp::epochMs() / 1000 - 1000);
        if (jwt.Verify(expired, "acq1", claims, error))
            throw std::runtime_error("expired JWT accepted");
        auto tampered = token;
        const auto first = tampered.find('.');
        tampered[first + 2] = tampered[first + 2] == 'A' ? 'B' : 'A';
        if (jwt.Verify(tampered, "acq1", claims, error))
            throw std::runtime_error("tampered JWT accepted");
        std::cout << "JWT issue, expiry, tamper and target-node checks passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
