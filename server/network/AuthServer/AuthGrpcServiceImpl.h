#pragma once
#include "ndt.grpc.pb.h"
#include "server/logic/AuthServer/LogicSystem.h"

namespace ndtapp::auth {
class AuthGrpcServiceImpl final : public ndt::AuthService::Service {
public:
    explicit AuthGrpcServiceImpl(LogicSystem &logic) : logic_(logic) {}
    grpc::Status Verify(grpc::ServerContext *, const ndt::LoginRequest *, ndt::LoginReply *) override;
private:
    LogicSystem &logic_;
};
}
