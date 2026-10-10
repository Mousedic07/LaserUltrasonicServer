#pragma once
#include "ndt.grpc.pb.h"
#include <memory>
#include <string>

namespace ndtapp::gate {
class AuthGrpcClient {
public:
    AuthGrpcClient();
    grpc::Status Verify(const std::string &username, const std::string &password,
                        ndt::LoginReply &reply) const;
private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<ndt::AuthService::Stub> stub_;
    int timeoutMs_;
};
}
